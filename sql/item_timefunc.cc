/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file defines all time functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <time.h>

/*
** Todo: Move month and days to language files
*/

#define MAX_DAY_NUMBER 3652424L

static String month_names[] = 
{ 
  String("January",	&my_charset_latin1), 
  String("February",	&my_charset_latin1),
  String("March",	&my_charset_latin1),
  String("April",	&my_charset_latin1),
  String("May",		&my_charset_latin1),
  String("June",	&my_charset_latin1),
  String("July",	&my_charset_latin1),
  String("August",	&my_charset_latin1),
  String("September",	&my_charset_latin1),
  String("October",	&my_charset_latin1),
  String("November",	&my_charset_latin1),
  String("December",	&my_charset_latin1)
};
static String day_names[] = 
{ 
  String("Monday",	&my_charset_latin1),
  String("Tuesday",	&my_charset_latin1),
  String("Wednesday",	&my_charset_latin1),
  String("Thursday",	&my_charset_latin1),
  String("Friday",	&my_charset_latin1),
  String("Saturday",	&my_charset_latin1),
  String("Sunday",	&my_charset_latin1)
};

enum date_time_format_types 
{ 
  TIME_ONLY= 0, TIME_MICROSECOND,
  DATE_ONLY, DATE_TIME, DATE_TIME_MICROSECOND
};

typedef struct date_time_format 
{
  const char* format_str;
  uint length;
};

static struct date_time_format date_time_formats[]=
{
  {"%s%02d:%02d:%02d", 10},
  {"%s%02d:%02d:%02d.%06d", 17},
  {"%04d-%02d-%02d", 10},
  {"%04d-%02d-%02d %02d:%02d:%02d", 19},
  {"%04d-%02d-%02d %02d:%02d:%02d.%06d", 26}
};


/*
  OPTIMIZATION TODO:
   - Replace the switch with a function that should be called for each
     date type.
   - Remove sprintf and opencode the conversion, like we do in
     Field_datetime.
*/

String *make_datetime(String *str, TIME *ltime, 
		      enum date_time_format_types format)
{
  char *buff;
  CHARSET_INFO *cs= &my_charset_bin;
  uint length= date_time_formats[format].length + 32;
  const char* format_str= date_time_formats[format].format_str;

  if (str->alloc(length))
    return 0;

  buff= (char*) str->ptr();
  switch (format) {
  case TIME_ONLY:
    length= cs->cset->snprintf(cs, buff, length, format_str, ltime->neg ? "-" : "",
			       ltime->hour, ltime->minute, ltime->second);
    break;
  case TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length, format_str, ltime->neg ? "-" : "",
			       ltime->hour, ltime->minute, ltime->second,
			       ltime->second_part);
    break;
  case DATE_ONLY:
    length= cs->cset->snprintf(cs, buff, length, format_str,
			       ltime->year, ltime->month, ltime->day);
    break;
  case DATE_TIME:
    length= cs->cset->snprintf(cs, buff, length, format_str,
			       ltime->year, ltime->month, ltime->day,
			       ltime->hour, ltime->minute, ltime->second);
    break;
  case DATE_TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length, format_str,
			       ltime->year, ltime->month, ltime->day,
			       ltime->hour, ltime->minute, ltime->second,
			       ltime->second_part);
    break;
  default:
    return 0;
  }

  str->length(length);
  str->set_charset(cs);
  return str;
}

/*
** Get a array of positive numbers from a string object.
** Each number is separated by 1 non digit character
** Return error if there is too many numbers.
** If there is too few numbers, assume that the numbers are left out
** from the high end. This allows one to give:
** DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.
*/

bool get_interval_info(const char *str,uint length,CHARSET_INFO *cs,
			uint count, long *values)
{
  const char *end=str+length;
  uint i;
  while (str != end && !my_isdigit(cs,*str))
    str++;

  for (i=0 ; i < count ; i++)
  {
    long value;
    for (value=0; str != end && my_isdigit(cs,*str) ; str++)
      value=value*10L + (long) (*str - '0');
    values[i]= value;
    while (str != end && !my_isdigit(cs,*str))
      str++;
    if (str == end && i != count-1)
    {
      i++;
      /* Change values[0...i-1] -> values[0...count-1] */
      bmove_upp((char*) (values+count), (char*) (values+i),
		sizeof(long)*i);
      bzero((char*) values, sizeof(long)*(count-i));
      break;
    }
  }
  return (str != end);
}

longlong Item_func_period_add::val_int()
{
  ulong period=(ulong) args[0]->val_int();
  int months=(int) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value) ||
      period == 0L)
    return 0; /* purecov: inspected */
  return (longlong)
    convert_month_to_period((uint) ((int) convert_period_to_month(period)+
				    months));
}


longlong Item_func_period_diff::val_int()
{
  ulong period1=(ulong) args[0]->val_int();
  ulong period2=(ulong) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return (longlong) ((long) convert_period_to_month(period1)-
		     (long) convert_period_to_month(period2));
}



longlong Item_func_to_days::val_int()
{
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day);
}

longlong Item_func_dayofyear::val_int()
{
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day) -
    calc_daynr(ltime.year,1,1) + 1;
}

longlong Item_func_dayofmonth::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.day;
}

longlong Item_func_month::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.month;
}

String* Item_func_monthname::val_str(String* str)
{
  uint   month=(uint) Item_func_month::val_int();
  if (!month)					// This is also true for NULL
  {
    null_value=1;
    return (String*) 0;
  }
  null_value=0;
  
  String *m=&month_names[month-1];
  str->copy(m->ptr(), m->length(), m->charset(), default_charset());
  return str;
}

// Returns the quarter of the year

longlong Item_func_quarter::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ((ltime.month+2)/3);
}

longlong Item_func_hour::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.hour;
}

longlong Item_func_minute::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.minute;
}
// Returns the second in time_exp in the range of 0 - 59

longlong Item_func_second::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.second;
}


/*
  Returns the week of year.

  The bits in week_format has the following meaning:
    0	If not set:	USA format: Sunday is first day of week
        If set:		ISO format: Monday is first day of week
    1   If not set:	Week is in range 0-53
    	If set		Week is in range 1-53.
*/

longlong Item_func_week::val_int()
{
  uint year;
  uint week_format;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  week_format= (uint) args[1]->val_int();
  return (longlong) calc_week(&ltime, 
			      (week_format & 2) != 0,
			      (week_format & 1) == 0,
			      &year);
}


longlong Item_func_yearweek::val_int()
{
  uint year,week;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  week=calc_week(&ltime, 1, (args[1]->val_int() & 1) == 0, &year);
  return week+year*100;
}


/* weekday() has a automatic to_days() on item */

longlong Item_func_weekday::val_int()
{
  ulong tmp_value=(ulong) args[0]->val_int();
  if ((null_value=(args[0]->null_value || !tmp_value)))
    return 0; /* purecov: inspected */

  return (longlong) calc_weekday(tmp_value,odbc_type)+test(odbc_type);
}

String* Item_func_dayname::val_str(String* str)
{
  uint weekday=(uint) val_int();		// Always Item_func_daynr()
  if (null_value)
    return (String*) 0;
  
  String *d=&day_names[weekday];
  str->copy(d->ptr(), d->length(), d->charset(), default_charset());
  return str;
}


longlong Item_func_year::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.year;
}


longlong Item_func_unix_timestamp::val_int()
{
  if (arg_count == 0)
    return (longlong) current_thd->query_start();
  if (args[0]->type() == FIELD_ITEM)
  {						// Optimize timestamp field
    Field *field=((Item_field*) args[0])->field;
    if (field->type() == FIELD_TYPE_TIMESTAMP)
      return ((Field_timestamp*) field)->get_timestamp();
  }
  String *str=args[0]->val_str(&value);
  if ((null_value=args[0]->null_value))
  {
    return 0; /* purecov: inspected */
  }
  return (longlong) str_to_timestamp(str->ptr(),str->length());
}


longlong Item_func_time_to_sec::val_int()
{
  TIME ltime;
  longlong seconds;
  (void) get_arg0_time(&ltime);
  seconds=ltime.hour*3600L+ltime.minute*60+ltime.second;
  return ltime.neg ? -seconds : seconds;
}


/*
  Convert a string to a interval value
  To make code easy, allow interval objects without separators.
*/

static bool get_interval_value(Item *args,interval_type int_type,
			       String *str_value, INTERVAL *t)
{
  long array[5],value;
  const char *str;
  uint32 length;
  LINT_INIT(value);  LINT_INIT(str); LINT_INIT(length);
  CHARSET_INFO *cs=str_value->charset();

  bzero((char*) t,sizeof(*t));
  if ((int) int_type <= INTERVAL_MICROSECOND)
  {
    value=(long) args->val_int();
    if (args->null_value)
      return 1;
    if (value < 0)
    {
      t->neg=1;
      value= -value;
    }
  }
  else
  {
    String *res;
    if (!(res=args->val_str(str_value)))
      return (1);

    /* record negative intervalls in t->neg */
    str=res->ptr();
    const char *end=str+res->length();
    while (str != end && my_isspace(cs,*str))
      str++;
    if (str != end && *str == '-')
    {
      t->neg=1;
      str++;
    }
    length=(uint32) (end-str);		// Set up pointers to new str
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    t->year=value;
    break;
  case INTERVAL_QUARTER:
    t->month=value*3;
    break;
  case INTERVAL_MONTH:
    t->month=value;
    break;
  case INTERVAL_WEEK:
    t->day=value*7;
    break;
  case INTERVAL_DAY:
    t->day=value;
    break;
  case INTERVAL_HOUR:
    t->hour=value;
    break;
  case INTERVAL_MICROSECOND:
    t->second_part=value;
    break;
  case INTERVAL_MINUTE:
    t->minute=value;
    break;
  case INTERVAL_SECOND:
    t->second=value;
    break;
  case INTERVAL_YEAR_MONTH:			// Allow YEAR-MONTH YYYYYMM
    if (get_interval_info(str,length,cs,2,array))
      return (1);
    t->year=array[0];
    t->month=array[1];
    break;
  case INTERVAL_DAY_HOUR:
    if (get_interval_info(str,length,cs,2,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    break;
  case INTERVAL_DAY_MICROSECOND:
    if (get_interval_info(str,length,cs,5,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    t->minute=array[2];
    t->second=array[3];
    t->second_part=array[4];
    break;
  case INTERVAL_DAY_MINUTE:
    if (get_interval_info(str,length,cs,3,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    t->minute=array[2];
    break;
  case INTERVAL_DAY_SECOND:
    if (get_interval_info(str,length,cs,4,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    t->minute=array[2];
    t->second=array[3];
    break;
  case INTERVAL_HOUR_MICROSECOND:
    if (get_interval_info(str,length,cs,4,array))
      return (1);
    t->hour=array[0];
    t->minute=array[1];
    t->second=array[2];
    t->second_part=array[3];
    break;
  case INTERVAL_HOUR_MINUTE:
    if (get_interval_info(str,length,cs,2,array))
      return (1);
    t->hour=array[0];
    t->minute=array[1];
    break;
  case INTERVAL_HOUR_SECOND:
    if (get_interval_info(str,length,cs,3,array))
      return (1);
    t->hour=array[0];
    t->minute=array[1];
    t->second=array[2];
    break;
  case INTERVAL_MINUTE_MICROSECOND:
    if (get_interval_info(str,length,cs,3,array))
      return (1);
    t->minute=array[0];
    t->second=array[1];
    t->second_part=array[2];
    break;
  case INTERVAL_MINUTE_SECOND:
    if (get_interval_info(str,length,cs,2,array))
      return (1);
    t->minute=array[0];
    t->second=array[1];
    break;
  case INTERVAL_SECOND_MICROSECOND:
    if (get_interval_info(str,length,cs,2,array))
      return (1);
    t->second=array[0];
    t->second_part=array[1];
    break;
  }
  return 0;
}


String *Item_date::val_str(String *str)
{
  ulong value=(ulong) val_int();
  if (null_value)
    return (String*) 0;
  if (!value)					// zero daynr
  {
    str->copy("0000-00-00",10,&my_charset_latin1,default_charset());
    return str;
  }
  
  char tmpbuff[11];
  sprintf(tmpbuff,"%04d-%02d-%02d",
	  (int) (value/10000L) % 10000,
	  (int) (value/100)%100,
	  (int) (value%100));
  str->copy(tmpbuff,10,&my_charset_latin1,default_charset());
  return str;
}


int Item_date::save_in_field(Field *field, bool no_conversions)
{
  TIME ltime;
  timestamp_type t_type=TIMESTAMP_FULL;
  if (get_date(&ltime,1))
  {
    if (null_value)
      return set_field_to_null(field);
    t_type=TIMESTAMP_NONE;			// Error
  }
  field->set_notnull();
  field->store_time(&ltime,t_type);
  return 0;
}


longlong Item_func_from_days::val_int()
{
  longlong value=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  uint year,month,day;
  get_date_from_daynr((long) value,&year,&month,&day);
  return (longlong) (year*10000L+month*100+day);
}


void Item_func_curdate::fix_length_and_dec()
{
  struct tm start;
  
  collation.set(default_charset());
  decimals=0; 
  max_length=10*default_charset()->mbmaxlen;

  store_now_in_tm(current_thd->query_start(),&start);
  
  value=(longlong) ((ulong) ((uint) start.tm_year+1900)*10000L+
		    ((uint) start.tm_mon+1)*100+
		    (uint) start.tm_mday);
  /* For getdate */
  ltime.year=	start.tm_year+1900;
  ltime.month=	start.tm_mon+1;
  ltime.day=	start.tm_mday;
  ltime.hour=	0;
  ltime.minute=	0;
  ltime.second=	0;
  ltime.second_part=0;
  ltime.neg=0;
  ltime.time_type=TIMESTAMP_DATE;
}


bool Item_func_curdate::get_date(TIME *res,
				 bool fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


/*
    Converts time in time_t to struct tm represenatation for local timezone.
    Defines timezone (local) used for whole CURDATE function
*/
void Item_func_curdate_local::store_now_in_tm(time_t now, struct tm *now_tm)
{
  localtime_r(&now,now_tm);
}


/*
    Converts time in time_t to struct tm represenatation for UTC
    Defines timezone (UTC) used for whole UTC_DATE function
*/
void Item_func_curdate_utc::store_now_in_tm(time_t now, struct tm *now_tm)
{
  gmtime_r(&now,now_tm);
}


String *Item_func_curtime::val_str(String *str)
{ 
  str_value.set(buff,buff_length,default_charset());
  return &str_value;
}

void Item_func_curtime::fix_length_and_dec()
{
  struct tm start;
  CHARSET_INFO *cs= default_charset();

  decimals=0; 
  max_length=8*cs->mbmaxlen;
  collation.set(cs);
  
  store_now_in_tm(current_thd->query_start(),&start);
  
  value=(longlong) ((ulong) ((uint) start.tm_hour)*10000L+
		    (ulong) (((uint) start.tm_min)*100L+
			     (uint) start.tm_sec));

  buff_length=cs->cset->snprintf(cs,buff,sizeof(buff),"%02d:%02d:%02d",
				 (int) start.tm_hour,
				 (int) start.tm_min,
				 (int) start.tm_sec);
}


/*
    Converts time in time_t to struct tm represenatation for local timezone.
    Defines timezone (local) used for whole CURTIME function
*/
void Item_func_curtime_local::store_now_in_tm(time_t now, struct tm *now_tm)
{
  localtime_r(&now,now_tm);
}


/*
    Converts time in time_t to struct tm represenatation for UTC.
    Defines timezone (UTC) used for whole UTC_TIME function
*/
void Item_func_curtime_utc::store_now_in_tm(time_t now, struct tm *now_tm)
{
  gmtime_r(&now,now_tm);
}


String *Item_func_now::val_str(String *str)
{
  str_value.set(buff,buff_length,default_charset());
  return &str_value;
}


void Item_func_now::fix_length_and_dec()
{
  struct tm start;
  CHARSET_INFO *cs= &my_charset_bin;
  
  decimals=0;
  max_length=19*cs->mbmaxlen;
  collation.set(cs);

  store_now_in_tm(current_thd->query_start(),&start);
  
  value=((longlong) ((ulong) ((uint) start.tm_year+1900)*10000L+
		     (((uint) start.tm_mon+1)*100+
		      (uint) start.tm_mday))*(longlong) 1000000L+
	 (longlong) ((ulong) ((uint) start.tm_hour)*10000L+
		     (ulong) (((uint) start.tm_min)*100L+
			    (uint) start.tm_sec)));
  
  buff_length= (uint) cs->cset->snprintf(cs,buff, sizeof(buff),
				   "%04d-%02d-%02d %02d:%02d:%02d",
				   ((int) (start.tm_year+1900)) % 10000,
				   (int) start.tm_mon+1,
				   (int) start.tm_mday,
				   (int) start.tm_hour,
				   (int) start.tm_min,
				   (int) start.tm_sec);
  /* For getdate */
  ltime.year=	start.tm_year+1900;
  ltime.month=	start.tm_mon+1;
  ltime.day=	start.tm_mday;
  ltime.hour=	start.tm_hour;
  ltime.minute=	start.tm_min;
  ltime.second=	start.tm_sec;
  ltime.second_part=0;
  ltime.neg=0;
  ltime.time_type=TIMESTAMP_FULL;
}

bool Item_func_now::get_date(TIME *res,
			     bool fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


int Item_func_now::save_in_field(Field *to, bool no_conversions)
{
  to->set_notnull();
  to->store_time(&ltime,TIMESTAMP_FULL);
  return 0;
}


/*
    Converts time in time_t to struct tm represenatation for local timezone.
    Defines timezone (local) used for whole CURRENT_TIMESTAMP function
*/
void Item_func_now_local::store_now_in_tm(time_t now, struct tm *now_tm)
{
  localtime_r(&now,now_tm);
}


/*
    Converts time in time_t to struct tm represenatation for UTC.
    Defines timezone (UTC) used for whole UTC_TIMESTAMP function
*/
void Item_func_now_utc::store_now_in_tm(time_t now, struct tm *now_tm)
{
  gmtime_r(&now,now_tm);
}


String *Item_func_sec_to_time::val_str(String *str)
{
  char buff[23*2];
  const char *sign="";
  longlong seconds=(longlong) args[0]->val_int();
  ulong length;
  if ((null_value=args[0]->null_value))
    return (String*) 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    sign= "-";
  }
  uint sec= (uint) ((ulonglong) seconds % 3600);
  length= my_sprintf(buff,(buff,"%s%02lu:%02u:%02u",sign,(long) (seconds/3600),
			   sec/60, sec % 60));
  str->copy(buff, length, &my_charset_latin1, default_charset());
  return str;
}


longlong Item_func_sec_to_time::val_int()
{
  longlong seconds=args[0]->val_int();
  longlong sign=1;
  if ((null_value=args[0]->null_value))
    return 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    sign= -1;
  }
  return sign*((seconds / 3600)*10000+((seconds/60) % 60)*100+ (seconds % 60));
}


void Item_func_date_format::fix_length_and_dec()
{
  decimals=0;
  if (args[1]->type() == STRING_ITEM)
  {						// Optimize the normal case
    fixed_length=1;
    max_length=format_length(((Item_string*) args[1])->const_string());
  }
  else
  {
    fixed_length=0;
    max_length=args[1]->max_length*10;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null=1;					// If wrong date
}


uint Item_func_date_format::format_length(const String *format)
{
  uint size=0;
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();

  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr == end-1)
      size++;
    else
    {
      switch(*++ptr) {
      case 'M': /* month, textual */
      case 'W': /* day (of the week), textual */
	size += 9;
	break;
      case 'D': /* day (of the month), numeric plus english suffix */
      case 'Y': /* year, numeric, 4 digits */
      case 'x': /* Year, used with 'v' */
      case 'X': /* Year, used with 'v, where week starts with Monday' */
	size += 4;
	break;
      case 'a': /* locale's abbreviated weekday name (Sun..Sat) */
      case 'b': /* locale's abbreviated month name (Jan.Dec) */
      case 'j': /* day of year (001..366) */
	size += 3;
	break;
      case 'U': /* week (00..52) */
      case 'u': /* week (00..52), where week starts with Monday */
      case 'V': /* week 1..53 used with 'x' */
      case 'v': /* week 1..53 used with 'x', where week starts with Monday */
      case 'H': /* hour (00..23) */
      case 'y': /* year, numeric, 2 digits */
      case 'm': /* month, numeric */
      case 'd': /* day (of the month), numeric */
      case 'h': /* hour (01..12) */
      case 'I': /* --||-- */
      case 'i': /* minutes, numeric */
      case 'k': /* hour ( 0..23) */
      case 'l': /* hour ( 1..12) */
      case 'p': /* locale's AM or PM */
      case 'S': /* second (00..61) */
      case 's': /* seconds, numeric */
      case 'c': /* month (0..12) */
      case 'e': /* day (0..31) */
	size += 2;
	break;
      case 'r': /* time, 12-hour (hh:mm:ss [AP]M) */
	size += 11;
	break;
      case 'T': /* time, 24-hour (hh:mm:ss) */
	size += 8;
	break;
      case 'f': /* microseconds */
	size += 6;
	break;
      case 'w': /* day (of the week), numeric */
      case '%':
      default:
	size++;
	break;
      }
    }
  }
  return size;
}


String *Item_func_date_format::val_str(String *str)
{
  String *format;
  TIME l_time;
  char intbuff[15];
  uint size,weekday;
  ulong length;

  if (!date_or_time)
  {
    if (get_arg0_date(&l_time,1))
      return 0;
  }
  else
  {
    String *res;
    if (!(res=args[0]->val_str(str)))
    {
      null_value=1;
      return 0;
    }
    if (str_to_time(res->ptr(),res->length(),&l_time))
    {
      null_value=1;
      return 0;
    }
    l_time.year=l_time.month=l_time.day=0;
    null_value=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
  {
    null_value=1;
    return 0;
  }

  if (fixed_length)
    size=max_length;
  else
    size=format_length(format);
  if (format == str)
    str= &value;				// Save result here
  if (str->alloc(size))
  {
    null_value=1;
    return 0;
  }
  str->length(0);

  /* Create the result string */
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();
  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr+1 == end)
      str->append(*ptr);
    else
    {
      switch (*++ptr) {
      case 'M':
	if (!l_time.month)
	{
	  null_value=1;
	  return 0;
	}
	str->append(month_names[l_time.month-1]);
	break;
      case 'b':
	if (!l_time.month)
	{
	  null_value=1;
	  return 0;
	}
	str->append(month_names[l_time.month-1].ptr(),3);
	break;
      case 'W':
	if (date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),0);
	str->append(day_names[weekday]);
	break;
      case 'a':
	if (date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),0);
	str->append(day_names[weekday].ptr(),3);
	break;
      case 'D':
	if (date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	length= int10_to_str(l_time.day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	if (l_time.day >= 10 &&  l_time.day <= 19)
	  str->append("th");
	else
	{
	  switch (l_time.day %10) {
	  case 1:
	    str->append("st",2);
	    break;
	  case 2:
	    str->append("nd",2);
	    break;
	  case 3:
	    str->append("rd",2);
	    break;
	  default:
	    str->append("th",2);
	    break;
	  }
	}
	break;
      case 'Y':
	length= int10_to_str(l_time.year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
	break;
      case 'y':
	length= int10_to_str(l_time.year%100, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'm':
	length= int10_to_str(l_time.month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'c':
	length= int10_to_str(l_time.month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'd':
	length= int10_to_str(l_time.day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'e':
	length= int10_to_str(l_time.day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'f':
	length= int10_to_str(l_time.second_part, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 6, '0');
	break;
      case 'H':
	length= int10_to_str(l_time.hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'h':
      case 'I':
	length= int10_to_str((l_time.hour+11)%12+1, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'i':					/* minutes */
	length= int10_to_str(l_time.minute, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'j':
	if (date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	length= int10_to_str(calc_daynr(l_time.year,l_time.month,l_time.day) - 
		     calc_daynr(l_time.year,1,1) + 1, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 3, '0');
	break;
      case 'k':
	length= int10_to_str(l_time.hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'l':
	length= int10_to_str((l_time.hour+11)%12+1, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'p':
	str->append(l_time.hour < 12 ? "AM" : "PM",2);
	break;
      case 'r':
	length= my_sprintf(intbuff, 
		   (intbuff, 
		    (l_time.hour < 12) ? "%02d:%02d:%02d AM" : "%02d:%02d:%02d PM",
		    (l_time.hour+11)%12+1,
		    l_time.minute,
		    l_time.second));
	str->append(intbuff, length);
	break;
      case 'S':
      case 's':
	length= int10_to_str(l_time.second, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'T':
	length= my_sprintf(intbuff, 
		   (intbuff, 
		    "%02d:%02d:%02d", 
		    l_time.hour, 
		    l_time.minute,
		    l_time.second));
	str->append(intbuff, length);
	break;
      case 'U':
      case 'u':
      {
	uint year;
	length= int10_to_str(calc_week(&l_time, 0, (*ptr) == 'U', &year),
		     intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'v':
      case 'V':
      {
	uint year;
	length= int10_to_str(calc_week(&l_time, 1, (*ptr) == 'V', &year),
		     intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'x':
      case 'X':
      {
	uint year;
	(void) calc_week(&l_time, 1, (*ptr) == 'X', &year);
	length= int10_to_str(year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
      }
      break;
      case 'w':
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),1);
	length= int10_to_str(weekday, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');

	break;
      default:
	str->append(*ptr);
	break;
      }
    }
  }
  return str;
}


String *Item_func_from_unixtime::val_str(String *str)
{
  struct tm tm_tmp,*start;
  time_t tmp=(time_t) args[0]->val_int();
  uint32 l;
  CHARSET_INFO *cs=default_charset();
  
  if ((null_value=args[0]->null_value))
    return 0;
  localtime_r(&tmp,&tm_tmp);
  start=&tm_tmp;
  
  l=20*cs->mbmaxlen+32;
  if (str->alloc(l))
    return str;					/* purecov: inspected */
  l=cs->cset->snprintf(cs,(char*) str->ptr(),l,"%04d-%02d-%02d %02d:%02d:%02d",
	  (int) start->tm_year+1900,
	  (int) start->tm_mon+1,
	  (int) start->tm_mday,
	  (int) start->tm_hour,
	  (int) start->tm_min,
	  (int) start->tm_sec);
  str->length(l);
  str->set_charset(cs);
  return str;
}


longlong Item_func_from_unixtime::val_int()
{
  time_t tmp=(time_t) (ulong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  struct tm tm_tmp,*start;
  localtime_r(&tmp,&tm_tmp);
  start= &tm_tmp;
  return ((longlong) ((ulong) ((uint) start->tm_year+1900)*10000L+
		      (((uint) start->tm_mon+1)*100+
		       (uint) start->tm_mday))*LL(1000000)+
	  (longlong) ((ulong) ((uint) start->tm_hour)*10000L+
		      (ulong) (((uint) start->tm_min)*100L+
			       (uint) start->tm_sec)));
}

bool Item_func_from_unixtime::get_date(TIME *ltime,
				       bool fuzzy_date __attribute__((unused)))
{
  time_t tmp=(time_t) (ulong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 1;
  struct tm tm_tmp,*start;
  localtime_r(&tmp,&tm_tmp);
  start= &tm_tmp;
  ltime->year=	start->tm_year+1900;
  ltime->month=	start->tm_mon+1;
  ltime->day=	start->tm_mday;
  ltime->hour=	start->tm_hour;
  ltime->minute=start->tm_min;
  ltime->second=start->tm_sec;
  ltime->second_part=0;
  ltime->neg=0;
  return 0;
}


void Item_date_add_interval::fix_length_and_dec()
{
  enum_field_types arg0_field_type;
  collation.set(default_charset());
  maybe_null=1;
  max_length=26*MY_CHARSET_BIN_MB_MAXLEN;
  value.alloc(32);

  /*
    The field type for the result of an Item_date function is defined as
    follows:

    - If first arg is a MYSQL_TYPE_DATETIME result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_DATE and the interval type uses hours,
      minutes or seconds then type is MYSQL_TYPE_DATETIME.
    - Otherwise the result is MYSQL_TYPE_STRING
      (This is because you can't know if the string contains a DATE, TIME or
      DATETIME argument)
  */
  cached_field_type= MYSQL_TYPE_STRING;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == MYSQL_TYPE_DATETIME ||
      arg0_field_type == MYSQL_TYPE_TIMESTAMP)
    cached_field_type= MYSQL_TYPE_DATETIME;
  else if (arg0_field_type == MYSQL_TYPE_DATE)
  {
    if (int_type <= INTERVAL_DAY || int_type == INTERVAL_YEAR_MONTH)
      cached_field_type= arg0_field_type;
    else
      cached_field_type= MYSQL_TYPE_DATETIME;
  }
}


/* Here arg[1] is a Item_interval object */

bool Item_date_add_interval::get_date(TIME *ltime, bool fuzzy_date)
{
  long period,sign;
  INTERVAL interval;

  if (args[0]->get_date(ltime,0) ||
      get_interval_value(args[1],int_type,&value,&interval))
    goto null_date;
  sign= (interval.neg ? -1 : 1);
  if (date_sub_interval)
    sign = -sign;

  null_value=0;
  switch (int_type) {
  case INTERVAL_SECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
  case INTERVAL_MINUTE:
  case INTERVAL_HOUR:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_DAY_SECOND:
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_DAY_HOUR:
    long sec,days,daynr,microseconds,extra_sec;
    ltime->time_type=TIMESTAMP_FULL;		// Return full date
    microseconds= ltime->second_part + sign*interval.second_part;
    extra_sec= microseconds/1000000L;
    microseconds= microseconds%1000000L;

    sec=((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
	 ltime->second +
	 sign*(interval.day*3600*24L +
	       interval.hour*3600+interval.minute*60+interval.second))+
      extra_sec;

    if (microseconds < 0)
    {
      microseconds+= 1000000L;	
      sec--;
    }
    days=sec/(3600*24L); sec=sec-days*3600*24L;
    if (sec < 0)
    {
      days--;
      sec+=3600*24L;
    }
    ltime->second_part= microseconds;
    ltime->second=sec % 60;
    ltime->minute=sec/60 % 60;
    ltime->hour=sec/3600;
    daynr= calc_daynr(ltime->year,ltime->month,1) + days;
    get_date_from_daynr(daynr,&ltime->year,&ltime->month,&ltime->day);
    if (daynr < 0 || daynr >= MAX_DAY_NUMBER) // Day number from year 0 to 9999-12-31
      goto null_date;
    break;
  case INTERVAL_DAY:
  case INTERVAL_WEEK:
    period= calc_daynr(ltime->year,ltime->month,ltime->day) +
      sign*interval.day;
    if (period < 0 || period >= MAX_DAY_NUMBER) // Daynumber from year 0 to 9999-12-31
      goto null_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year += sign*interval.year;
    if ((int) ltime->year < 0 || ltime->year >= 10000L)
      goto null_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign*interval.year*12 +
	     ltime->month-1 + sign*interval.month);
    if (period < 0 || period >= 120000L)
      goto null_date;
    ltime->year= (uint) (period / 12);
    ltime->month= (uint) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day = days_in_month[ltime->month-1];
      if (ltime->month == 2 && calc_days_in_year(ltime->year) == 366)
	ltime->day++;				// Leap-year
    }
    break;
  default:
    goto null_date;
  }
  return 0;					// Ok

 null_date:
  return (null_value=1);
}


String *Item_date_add_interval::val_str(String *str)
{
  TIME ltime;
  enum date_time_format_types format;

  if (Item_date_add_interval::get_date(&ltime,0))
    return 0;

  if (ltime.time_type == TIMESTAMP_DATE)
    format= DATE_ONLY;
  else if (ltime.second_part)
    format= DATE_TIME_MICROSECOND;
  else
    format= DATE_TIME;

  if (make_datetime(str, &ltime, format))
    return str;

  null_value=1;
  return 0;
}

longlong Item_date_add_interval::val_int()
{
  TIME ltime;
  longlong date;
  if (Item_date_add_interval::get_date(&ltime,0))
    return (longlong) 0;
  date = (ltime.year*100L + ltime.month)*100L + ltime.day;
  return ltime.time_type == TIMESTAMP_DATE ? date :
    ((date*100L + ltime.hour)*100L+ ltime.minute)*100L + ltime.second;
}

void Item_extract::fix_length_and_dec()
{
  value.alloc(32);				// alloc buffer

  maybe_null=1;					// If wrong date
  switch (int_type) {
  case INTERVAL_YEAR:		max_length=4; date_value=1; break;
  case INTERVAL_YEAR_MONTH:	max_length=6; date_value=1; break;
  case INTERVAL_QUARTER:        max_length=2; date_value=1; break;
  case INTERVAL_MONTH:		max_length=2; date_value=1; break;
  case INTERVAL_WEEK:		max_length=2; date_value=1; break;
  case INTERVAL_DAY:		max_length=2; date_value=1; break;
  case INTERVAL_DAY_HOUR:	max_length=9; date_value=0; break;
  case INTERVAL_DAY_MINUTE:	max_length=11; date_value=0; break;
  case INTERVAL_DAY_SECOND:	max_length=13; date_value=0; break;
  case INTERVAL_HOUR:		max_length=2; date_value=0; break;
  case INTERVAL_HOUR_MINUTE:	max_length=4; date_value=0; break;
  case INTERVAL_HOUR_SECOND:	max_length=6; date_value=0; break;
  case INTERVAL_MINUTE:		max_length=2; date_value=0; break;
  case INTERVAL_MINUTE_SECOND:	max_length=4; date_value=0; break;
  case INTERVAL_SECOND:		max_length=2; date_value=0; break;
  case INTERVAL_MICROSECOND:	max_length=2; date_value=0; break;
  case INTERVAL_DAY_MICROSECOND: max_length=20; date_value=0; break;
  case INTERVAL_HOUR_MICROSECOND: max_length=13; date_value=0; break;
  case INTERVAL_MINUTE_MICROSECOND: max_length=11; date_value=0; break;
  case INTERVAL_SECOND_MICROSECOND: max_length=9; date_value=0; break;
  }
}


longlong Item_extract::val_int()
{
  TIME ltime;
  uint year;
  ulong week_format;
  long neg;
  if (date_value)
  {
    if (get_arg0_date(&ltime,1))
      return 0;
    neg=1;
  }
  else
  {
    String *res= args[0]->val_str(&value);
    if (!res || str_to_time(res->ptr(),res->length(),&ltime))
    {
      null_value=1;
      return 0;
    }
    neg= ltime.neg ? -1 : 1;
    null_value=0;
  }

  switch (int_type) {
  case INTERVAL_YEAR:		return ltime.year;
  case INTERVAL_YEAR_MONTH:	return ltime.year*100L+ltime.month;
  case INTERVAL_QUARTER:	return ltime.month/3 + 1;
  case INTERVAL_MONTH:		return ltime.month;
  case INTERVAL_WEEK:
  {
    week_format= current_thd->variables.default_week_format;
    return calc_week(&ltime, 
		     (week_format & 2) != 0,
		     (week_format & 1) == 0,
		     &year);
  }
  case INTERVAL_DAY:		return ltime.day;
  case INTERVAL_DAY_HOUR:	return (long) (ltime.day*100L+ltime.hour)*neg;
  case INTERVAL_DAY_MINUTE:	return (long) (ltime.day*10000L+
					       ltime.hour*100L+
					       ltime.minute)*neg;
  case INTERVAL_DAY_SECOND:	 return ((longlong) ltime.day*1000000L+
					 (longlong) (ltime.hour*10000L+
						     ltime.minute*100+
						     ltime.second))*neg;
  case INTERVAL_HOUR:		return (long) ltime.hour*neg;
  case INTERVAL_HOUR_MINUTE:	return (long) (ltime.hour*100+ltime.minute)*neg;
  case INTERVAL_HOUR_SECOND:	return (long) (ltime.hour*10000+ltime.minute*100+
					       ltime.second)*neg;
  case INTERVAL_MINUTE:		return (long) ltime.minute*neg;
  case INTERVAL_MINUTE_SECOND:	return (long) (ltime.minute*100+ltime.second)*neg;
  case INTERVAL_SECOND:		return (long) ltime.second*neg;
  case INTERVAL_MICROSECOND:	return (long) ltime.second_part*neg;
  case INTERVAL_DAY_MICROSECOND: return (((longlong)ltime.day*1000000L +
					  (longlong)ltime.hour*10000L +
					  ltime.minute*100 +
					  ltime.second)*1000000L +
					 ltime.second_part)*neg;
  case INTERVAL_HOUR_MICROSECOND: return (((longlong)ltime.hour*10000L +
					   ltime.minute*100 +
					   ltime.second)*1000000L +
					  ltime.second_part)*neg;
  case INTERVAL_MINUTE_MICROSECOND: return (((longlong)(ltime.minute*100+
							ltime.second))*1000000L+
					    ltime.second_part)*neg;
  case INTERVAL_SECOND_MICROSECOND: return ((longlong)ltime.second*1000000L+
					    ltime.second_part)*neg;
  }
  return 0;					// Impossible
}

bool Item_extract::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_extract* ie= (Item_extract*)item;
  if (ie->int_type != int_type)
    return 0;

  if (!args[0]->eq(ie->args[0], binary_cmp))
      return 0;
  return 1;
}

void Item_typecast::print(String *str)
{
  str->append("CAST(");
  args[0]->print(str);
  str->append(" AS ");
  str->append(func_name());
  str->append(')');
}

String *Item_char_typecast::val_str(String *str)
{
  String *res, *res1;
  uint32 length;

  if (!charset_conversion && !(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }
  else
  {
    // Convert character set if differ
    if (!(res1= args[0]->val_str(&tmp_value)) ||
	str->copy(res1->ptr(), res1->length(),res1->charset(), cast_cs))
    {
      null_value= 1;
      return 0;
    }
    res= str;
  }
  
  /*
     Cut the tail if cast with length
     and the result is longer than cast length, e.g.
     CAST('string' AS CHAR(1))
  */
  if (cast_length >= 0 && 
      (res->length() > (length= (uint32) res->charpos(cast_length))))
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      res= &str_value;
    }
    res->length((uint) length);
  } 
  null_value= 0;
  return res;
}

void Item_char_typecast::fix_length_and_dec()
{
  uint32 char_length;
  charset_conversion= !my_charset_same(args[0]->collation.collation, cast_cs) &&
		      args[0]->collation.collation != &my_charset_bin &&
		      cast_cs != &my_charset_bin;
  collation.set(cast_cs, DERIVATION_IMPLICIT);
  char_length= (cast_length >= 0) ? cast_length : 
	       args[0]->max_length/args[0]->collation.collation->mbmaxlen;
  max_length= char_length * cast_cs->mbmaxlen;
}

String *Item_datetime_typecast::val_str(String *str)
{
  TIME ltime;

  if (!get_arg0_date(&ltime,1) &&
      make_datetime(str, &ltime, ltime.second_part ?
	    DATE_TIME_MICROSECOND : DATE_TIME))
  return str;

null_date:
  null_value=1;
  return 0;
}


bool Item_time_typecast::get_time(TIME *ltime)
{
  bool res= get_arg0_time(ltime);
  ltime->time_type= TIMESTAMP_TIME;
  return res;
}


String *Item_time_typecast::val_str(String *str)
{
  TIME ltime;

  if (!get_arg0_time(&ltime) &&
      make_datetime(str, &ltime, ltime.second_part ? TIME_MICROSECOND : TIME_ONLY))
    return str;

  null_value=1;
  return 0;
}


bool Item_date_typecast::get_date(TIME *ltime, bool fuzzy_date)
{
  bool res= get_arg0_date(ltime,1);
  ltime->time_type= TIMESTAMP_DATE;
  return res;
}


String *Item_date_typecast::val_str(String *str)
{
  TIME ltime;

  if (!get_arg0_date(&ltime,1) &&
      make_datetime(str, &ltime, DATE_ONLY))
  return str;

null_date:
  null_value=1;
  return 0;
}

/*
  MAKEDATE(a,b) is a date function that creates a date value 
  from a year and day value.
*/

String *Item_func_makedate::val_str(String *str)
{
  TIME l_time;
  long daynr= args[1]->val_int();
  long yearnr= args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      yearnr < 0 || daynr <= 0)
    goto null_date;

  days= calc_daynr(yearnr,1,1) + daynr - 1;
  if (days > 0 || days < MAX_DAY_NUMBER) // Day number from year 0 to 9999-12-31
  {
    null_value=0;
    get_date_from_daynr(days,&l_time.year,&l_time.month,&l_time.day);
    if (make_datetime(str, &l_time, DATE_ONLY))
      return str;
  }

null_date:
  null_value=1;
  return 0;
}


void Item_func_add_time::fix_length_and_dec()
{
  enum_field_types arg0_field_type;
  decimals=0;
  max_length=26*MY_CHARSET_BIN_MB_MAXLEN;

  /*
    The field type for the result of an Item_func_add_time function is defined as
    follows:

    - If first arg is a MYSQL_TYPE_DATETIME or MYSQL_TYPE_TIMESTAMP 
      result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_TIME result is MYSQL_TYPE_TIME
    - Otherwise the result is MYSQL_TYPE_STRING
  */

  cached_field_type= MYSQL_TYPE_STRING;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == MYSQL_TYPE_DATE ||
      arg0_field_type == MYSQL_TYPE_DATETIME ||
      arg0_field_type == MYSQL_TYPE_TIMESTAMP)
    cached_field_type= MYSQL_TYPE_DATETIME;
  else if (arg0_field_type == MYSQL_TYPE_TIME)
    cached_field_type= MYSQL_TYPE_TIME;
}

/*
  ADDTIME(t,a) and SUBTIME(t,a) are time functions that calculate a time/datetime value 

  t: time_or_datetime_expression
  a: time_expression
  
  Result: Time value or datetime value
*/

String *Item_func_add_time::val_str(String *str)
{
  TIME l_time1, l_time2, l_time3;
  bool is_time= 0;
  long microseconds, seconds, days= 0;
  int l_sign= sign;

  null_value=0;
  l_time3.neg= 0;
  if (is_date)                        // TIMESTAMP function
  {
    if (get_arg0_date(&l_time1,1) || 
        args[1]->get_time(&l_time2) ||
        l_time1.time_type == TIMESTAMP_TIME || 
        l_time2.time_type != TIMESTAMP_TIME)
      goto null_date;
  }
  else                                // ADDTIME function
  {
    if (args[0]->get_time(&l_time1) || 
        args[1]->get_time(&l_time2) ||
        l_time2.time_type == TIMESTAMP_FULL)
      goto null_date;
    is_time= (l_time1.time_type == TIMESTAMP_TIME);
    if (is_time && (l_time2.neg == l_time1.neg && l_time1.neg))
      l_time3.neg= 1;
  }
  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  microseconds= l_time1.second_part + l_sign*l_time2.second_part;
  seconds= (l_time1.hour*3600L + l_time1.minute*60L + l_time1.second +
	    (l_time2.day*86400L + l_time2.hour*3600L +
	     l_time2.minute*60L + l_time2.second)*l_sign);
  if (is_time)
    seconds+= l_time1.day*86400L;
  else
    days+= calc_daynr((uint) l_time1.year,(uint) l_time1.month, (uint) l_time1.day);
  seconds= seconds + microseconds/1000000L;
  microseconds= microseconds%1000000L;
  days+= seconds/86400L;
  seconds= seconds%86400L;

  if (microseconds < 0)
  {
    microseconds+= 1000000L;
    seconds--;
  }
  if (seconds < 0)
  {
    days+= seconds/86400L - 1;
    seconds+= 86400L;
  }
  if (days < 0)
  {
    if (!is_time)
      goto null_date;
    if (microseconds)
    {
      microseconds= 1000000L - microseconds;
      seconds++; 
    }
    seconds= 86400L - seconds;
    days= -(++days);
    l_time3.neg= 1;
  }

  calc_time_from_sec(&l_time3, seconds, microseconds);
  if (!is_time)
  {
    get_date_from_daynr(days,&l_time3.year,&l_time3.month,&l_time3.day);
    if (l_time3.day &&
	make_datetime(str, &l_time3,
	              l_time1.second_part || l_time2.second_part ?
	              DATE_TIME_MICROSECOND : DATE_TIME))
      return str;
    goto null_date;
  }

  l_time3.hour+= days*24;
  if (make_datetime(str, &l_time3,
	    l_time1.second_part || l_time2.second_part ?
	    TIME_MICROSECOND : TIME_ONLY))
    return str;

null_date:
  null_value=1;
  return 0;
}

/*
  SYNOPSIS
    calc_time_diff()
    l_time1		TIME/DATE/DATETIME value
    l_time2		TIME/DATE/DATETIME value
    l_sign		Can be 1 (operation of addition)
                        or -1 (substraction)
    seconds_out         Returns count of seconds bitween
                        l_time1 and l_time2
    microseconds_out    Returns count of microseconds bitween
                        l_time1 and l_time2.

  DESCRIPTION
    Calculates difference in seconds(seconds_out)
    and microseconds(microseconds_out)
    bitween two TIME/DATE/DATETIME values.

  RETURN VALUES
    Rertuns sign of difference.
    1 means negative result
    0 means positive result

*/

bool calc_time_diff(TIME *l_time1,TIME *l_time2, int l_sign,
		    longlong *seconds_out, long *microseconds_out)
{
  long days;
  bool neg;
  longlong seconds= *seconds_out;
  long microseconds= *microseconds_out;

  /*
    We suppose that if first argument is TIMESTAMP_TIME
    the second argument should be TIMESTAMP_TIME also.
    We should check it before calc_time_diff call.
  */
  if (l_time1->time_type == TIMESTAMP_TIME)  // Time value
    days= l_time1->day - l_sign*l_time2->day;
  else                                      // DateTime value
    days= (calc_daynr((uint) l_time1->year,
		      (uint) l_time1->month,
		      (uint) l_time1->day) - 
	   l_sign*calc_daynr((uint) l_time2->year,
			     (uint) l_time2->month,
			     (uint) l_time2->day));

  microseconds= l_time1->second_part - l_sign*l_time2->second_part;
  seconds= ((longlong) days*86400L + l_time1->hour*3600L + 
	    l_time1->minute*60L + l_time1->second + microseconds/1000000L -
	    (longlong)l_sign*(l_time2->hour*3600L+l_time2->minute*60L+l_time2->second));

  neg= 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    neg= 1;
  }
  else if (seconds == 0 && microseconds < 0)
  {
    microseconds= -microseconds;
    neg= 1;
  }
  if (microseconds < 0)
  {
    microseconds+= 1000000L;
    seconds--;
  }
  *seconds_out= seconds;
  *microseconds_out= microseconds;
  return neg;
}

/*
  TIMEDIFF(t,s) is a time function that calculates the 
  time value between a start and end time.

  t and s: time_or_datetime_expression
  Result: Time value
*/

String *Item_func_timediff::val_str(String *str)
{
  longlong seconds;
  long microseconds;
  long days;
  int l_sign= 1;
  TIME l_time1 ,l_time2, l_time3;

  null_value= 0;  
  if (args[0]->get_time(&l_time1) ||
      args[1]->get_time(&l_time2) ||
      l_time1.time_type != l_time2.time_type)
    goto null_date;

  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  l_time3.neg= calc_time_diff(&l_time1,&l_time2, l_sign,
			      &seconds, &microseconds);

  /*
    For TIMESTAMP_TIME only:
      If both argumets are negative values and diff between them
      is negative we need to swap sign as result should be positive.
  */
  if ((l_time2.neg == l_time1.neg) && l_time1.neg)
    l_time3.neg= 1-l_time3.neg;         // Swap sign of result

  calc_time_from_sec(&l_time3, seconds, microseconds);
  if (make_datetime(str, &l_time3, 
	    l_time1.second_part || l_time2.second_part ?
	    TIME_MICROSECOND : TIME_ONLY))
    return str;

null_date:
  null_value=1;
  return 0;
}

/*
  MAKETIME(h,m,s) is a time function that calculates a time value 
  from the total number of hours, minutes, and seconds.
  Result: Time value
*/

String *Item_func_maketime::val_str(String *str)
{
  TIME ltime;

  long hour= args[0]->val_int();
  long minute= args[1]->val_int();
  long second= args[2]->val_int();

  if ((null_value=(args[0]->null_value || 
		   args[1]->null_value ||
		   args[2]->null_value || 
		   minute > 59 || minute < 0 || 
		   second > 59 || second < 0)))
    goto null_date;

  ltime.neg= 0;
  if (hour < 0)
  {
    ltime.neg= 1;
    hour= -hour;
  }
  ltime.hour= (ulong)hour;
  ltime.minute= (ulong)minute;
  ltime.second= (ulong)second;
  if (make_datetime(str, &ltime, TIME_ONLY))
    return str;

null_date:
    return 0;
}

/*
  MICROSECOND(a) is a function ( extraction) that extracts the microseconds from a.

  a: Datetime or time value
  Result: int value
*/
longlong Item_func_microsecond::val_int()
{
  TIME ltime;
  if (!get_arg0_time(&ltime))
    return ltime.second_part;
  return 0;
}


longlong Item_func_timestamp_diff::val_int()
{
  TIME ltime1, ltime2;
  longlong seconds;
  long microseconds;
  long months= 0;
  int neg= 1;

  null_value= 0;  
  if (args[0]->get_date(&ltime1, 0) ||
      args[1]->get_date(&ltime2, 0))
    goto null_date;

  if (calc_time_diff(&ltime2,&ltime1, 1,
		     &seconds, &microseconds))
    neg= -1;

  if (int_type == INTERVAL_YEAR ||
      int_type == INTERVAL_QUARTER ||
      int_type == INTERVAL_MONTH)
  {
    uint year, year_tmp;
    uint year_beg, year_end, month_beg, month_end;
    uint diff_days= seconds/86400L;
    uint diff_months= 0;
    uint diff_years= 0;
    if (neg == -1)
    {
      year_beg= ltime2.year;
      year_end= ltime1.year;
      month_beg= ltime2.month;
      month_end= ltime1.month;
    }
    else
    {
      year_beg= ltime1.year;
      year_end= ltime2.year;
      month_beg= ltime1.month;
      month_end= ltime2.month;
    }
    /* calc years*/
    for (year= year_beg;year < year_end; year++)
    {
      uint days=calc_days_in_year(year);
      if (days > diff_days)
	break;
      diff_days-= days;
      diff_years++;
    }

    /* calc months;  Current year is in the 'year' variable */
    month_beg--;	/* Change months to be 0-11 for easier calculation */
    month_end--;

    months= 12*diff_years;
    while (month_beg != month_end)
    {
      uint m_days= (uint) days_in_month[month_beg];
      if (month_beg == 1)
      {
	/* This is only calculated once so there is no reason to cache it*/
	uint leap= (uint) ((year & 3) == 0 && (year%100 ||
					       (year%400 == 0 && year)));
	m_days+= leap;
      }
      if (m_days > diff_days)
	break;
      diff_days-= m_days;
      months++;
      if (month_beg++ == 11)		/* if we wrap to next year */
      {
	month_beg= 0;
	year++;
      }
    }
    if (neg == -1)
      months= -months;
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    return months/12;
  case INTERVAL_QUARTER:
    return months/3;
  case INTERVAL_MONTH:
    return months;
  case INTERVAL_WEEK:          
    return seconds/86400L/7L*neg;
  case INTERVAL_DAY:		
    return seconds/86400L*neg;
  case INTERVAL_HOUR:		
    return seconds/3600L*neg;
  case INTERVAL_MINUTE:		
    return seconds/60L*neg;
  case INTERVAL_SECOND:		
    return seconds*neg;
  case INTERVAL_MICROSECOND:
  {
    longlong max_sec= LONGLONG_MAX/1000000;
    if (max_sec > seconds ||
	max_sec == seconds && LONGLONG_MAX%1000000 >= microseconds)
      return (longlong) (seconds*1000000L+microseconds)*neg;
    goto null_date;
  }
  default:
    break;
  }

null_date:
  null_value=1;
  return 0;
}


void Item_func_timestamp_diff::print(String *str)
{
  str->append(func_name());
  str->append('(');

  switch (int_type) {
  case INTERVAL_YEAR:
    str->append("YEAR");
    break;
  case INTERVAL_QUARTER:
    str->append("QUARTER");
    break;
  case INTERVAL_MONTH:
    str->append("MONTH");
    break;
  case INTERVAL_WEEK:          
    str->append("WEEK");
    break;
  case INTERVAL_DAY:		
    str->append("DAY");
    break;
  case INTERVAL_HOUR:
    str->append("HOUR");
    break;
  case INTERVAL_MINUTE:		
    str->append("MINUTE");
    break;
  case INTERVAL_SECOND:
    str->append("SECOND");
    break;		
  case INTERVAL_MICROSECOND:
    str->append("SECOND_FRAC");
    break;
  default:
    break;
  }

  for (uint i=0 ; i < 2 ; i++)
  {
    str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}
