/*
 * DS3232 RTC Module
 *
 * Provides access to the DS3232 Real-Time clock (both timekeeping and parameter RAM).
 *
 * Based on Jack Christensen's Arduino library
 *  https://github.com/JChristensen/DS3232RTC
 *
 * with routines from Michael Margolis' time.c file.
 *
 * Copyright 2020 Jack Christensen, Michael Margolis and Dan Julio
 *
 * This file is part of firecam.
 *
 * firecam is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * firecam is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with firecam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "ds3232.h"
#include "esp_log.h"
#include "i2c.h"


//
// RTC private constants
//

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+(Y))>0) && !((1970+(Y))%4) && ( ((1970+(Y))%100) || !((1970+(Y))%400) ) )

// Useful time constants
#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))
#define DAYS_PER_WEEK ((time_t)(7UL))
#define SECS_PER_WEEK ((time_t)(SECS_PER_DAY * DAYS_PER_WEEK))
#define SECS_PER_YEAR ((time_t)(SECS_PER_DAY * 365UL)) // TODO: ought to handle leap years
#define SECS_YR_2000  ((time_t)(946684800UL)) // the time at the start of y2k



//
// RTC variables
//
static const char* TAG = "RTC";

static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0



//
// RTC Module forward declarations for internal functions
//
static uint8_t dec2bcd(uint8_t n);
static uint8_t bcd2dec(uint8_t n);



//
// RTC API
//

/**
 * Read the current time from the RTC and return it as a time_t
 * value. Returns a zero value if an I2C error occurred (e.g. RTC
 * not present).
 */
time_t get_rtc_time_secs()
{
    tmElements_t tm;

    if ( read_rtc_time(&tm) ) return 0;
    return( rtc_makeTime(tm) );
}


/**
 * Set the RTC to the given time_t value and clear the
 * oscillator stop flag (OSF) in the Control/Status register.
 * Returns the I2C status (zero if successful).
 */
int set_rtc_time_secs(time_t t)
{
    tmElements_t tm;

    rtc_breakTime(t, &tm);
    return ( write_rtc_time(tm) );
}


/**
 * Read the current time from the RTC and return it in a tmElements_t
 * structure. Returns zero if successful.
 */
int read_rtc_time(tmElements_t* tm)
{
	uint8_t buf[tmNbrFields];
	
	// Attempt to read the time registers from the RTC chip
	if (read_rtc_bytes(RTC_SECONDS, buf, tmNbrFields)) {
		rtc_breakTime(0, tm);  // Set time to 0 (1970...)
		return 1;
	}

    // Convert chip register data (secs, min, hr, dow, date, mth, yr) to our time structure
    tm->Second = bcd2dec(buf[RTC_SECONDS] & ~_BV(DS1307_CH));
    tm->Minute = bcd2dec(buf[RTC_MINUTES]);
    tm->Hour = bcd2dec(buf[RTC_HOURS] & ~_BV(HR1224));    // assumes 24hr clock
    tm->Wday = buf[RTC_DAY];
    tm->Day = bcd2dec(buf[RTC_DATE]);
    tm->Month = bcd2dec(buf[RTC_MONTH] & ~_BV(CENTURY));  // don't use the Century bit
    tm->Year = y2kYearToTm(bcd2dec(buf[RTC_YEAR]));
    return 0;
}


/**
 * Set the RTC time from a tmElements_t structure and clear the
 * oscillator stop flag (OSF) in the Control/Status register.
 * Returns zero if successful.
 */
int write_rtc_time(tmElements_t tm)
{
	uint8_t buf[tmNbrFields+1];
	uint8_t status;
	
	// Convert our time structure to chip register values
	buf[0] = RTC_SECONDS;                        // Starting register address
	buf[RTC_SECONDS+1] = dec2bcd(tm.Second);
	buf[RTC_MINUTES+1] = dec2bcd(tm.Minute);
	buf[RTC_HOURS+1] = dec2bcd(tm.Hour);
	buf[RTC_DAY+1] = tm.Wday;
	buf[RTC_DATE+1] = dec2bcd(tm.Day);
	buf[RTC_MONTH+1] = dec2bcd(tm.Month);
	buf[RTC_YEAR+1] = dec2bcd(tmYearToY2k(tm.Year));

	// Attempt to update the RTC chip
	if (write_rtc_bytes(buf, tmNbrFields+1)) {
		return 1;
	}
	
	// Clear the Oscillator Stop Flag in the status register
	if (read_rtc_byte(RTC_STATUS, &status) == 0) {
		/*return*/ write_rtc_byte(RTC_STATUS, status & ~_BV(OSF));
	}
	
	read_rtc_bytes(RTC_SECONDS, buf, tmNbrFields);

	return 0;
}


/**
 * Write multiple bytes to the RTC via I2C.  First byte must be register address.
 * Returns zero if successful
 */
int write_rtc_bytes(uint8_t* values, uint8_t nBytes)
{
	esp_err_t ret;
	
	// Atomically perform the I2C access
	i2c_lock();
	ret = i2c_master_write_slave(RTC_ADDR, values, nBytes);
	i2c_unlock();
	
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "write_rtc_bytes failed");
		return 1;
	}
    
    return 0;
}


/**
 * Write a single byte to the RTC via I2C.
 * Returns zero if successful
 */ 
int write_rtc_byte(uint8_t addr, uint8_t value)
{
    esp_err_t ret;
    uint8_t buf[2];

	buf[0] = addr;
	buf[1] = value;
		
	// Atomically perform the I2C access
	i2c_lock();
	ret = i2c_master_write_slave(RTC_ADDR, buf, 2);
	i2c_unlock();
	
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "write_rtc_byte failed");
		return 1;
	}
    
    return 0;
}


/**
 * Read multiple bytes from the RTC via I2C.
 * Returns zero if successful
 */
int read_rtc_bytes(uint8_t addr, uint8_t* values, uint8_t nBytes)
{
    esp_err_t ret;
	
	// Atomically perform the I2C access
	i2c_lock();
	ret = i2c_master_write_slave(RTC_ADDR, &addr, 1);
	if (ret == ESP_OK) {
		ret = i2c_master_read_slave(RTC_ADDR, values, nBytes);
	}
	i2c_unlock();
	
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "read_rtc_bytes failed");
		return 1;
	}
    
    return 0;
}


/**
 * Read a signel byte from the RTC via I2C.
 * Returns zero if successful
 */
int read_rtc_byte(uint8_t addr, uint8_t* value)
{
    esp_err_t ret;
	
	// Atomically perform the I2C access
	i2c_lock();
	ret = i2c_master_write_slave(RTC_ADDR, &addr, 1);
	if (ret == ESP_OK) {
		ret = i2c_master_read_slave(RTC_ADDR, value, 1);
	}
	i2c_unlock();
	
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "read_rtc_byte failed");
		return 1;
	}
    
    return 0;
}


/**
 * Set an alarm time. Sets the alarm registers only.  To cause the
 * INT pin to be asserted on alarm match, use set_rtc_alarm_interrupt().
 * This method can set either Alarm 1 or Alarm 2, depending on the
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration).
 * When setting Alarm 2, the seconds value must be supplied but is
 * ignored, recommend using zero. (Alarm 2 has no seconds register.)
 */
void set_rtc_alarm_secs(enum ALARM_TYPES_t alarmType, uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t daydate)
{
    uint8_t addr;

    seconds = dec2bcd(seconds);
    minutes = dec2bcd(minutes);
    hours = dec2bcd(hours);
    daydate = dec2bcd(daydate);
    if (alarmType & 0x01) seconds |= _BV(A1M1);
    if (alarmType & 0x02) minutes |= _BV(A1M2);
    if (alarmType & 0x04) hours |= _BV(A1M3);
    if (alarmType & 0x10) daydate |= _BV(DYDT);
    if (alarmType & 0x08) daydate |= _BV(A1M4);

    if ( !(alarmType & 0x80) )  // alarm 1
    {
        addr = ALM1_SECONDS;
        write_rtc_byte(addr++, seconds);
    }
    else
    {
        addr = ALM2_MINUTES;
    }
    write_rtc_byte(addr++, minutes);
    write_rtc_byte(addr++, hours);
    write_rtc_byte(addr++, daydate);
}


/**
 * Set an alarm time. Sets the alarm registers only. To cause the
 * INT pin to be asserted on alarm match, use set_rtc_alarm_interrupt().
 * This method can set either Alarm 1 or Alarm 2, depending on the
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration).
 * However, when using this method to set Alarm 1, the seconds value
 * is set to zero. (Alarm 2 has no seconds register.)
 */
void set_rtc_alarm(enum ALARM_TYPES_t alarmType, uint8_t minutes, uint8_t hours, uint8_t daydate)
{
    set_rtc_alarm_secs(alarmType, 0, minutes, hours, daydate);
}


/**
 * Enable or disable an alarm "interrupt" which asserts the INT pin
 * on the RTC.
 */
void set_rtc_alarm_interrupt(uint8_t alarmNumber, bool interruptEnabled)
{
    uint8_t controlReg, mask;

    read_rtc_byte(RTC_CONTROL, &controlReg);
    mask = _BV(A1IE) << (alarmNumber - 1);
    if (interruptEnabled)
        controlReg |= mask;
    else
        controlReg &= ~mask;
    write_rtc_byte(RTC_CONTROL, controlReg);
}


/**
 * Returns true or false depending on whether the given alarm has been
 * triggered, and resets the alarm flag bit.
 */
bool is_rtc_alarm(uint8_t alarmNumber)
{
    uint8_t statusReg, mask;

    read_rtc_byte(RTC_STATUS, &statusReg);
    mask = _BV(A1F) << (alarmNumber - 1);
    if (statusReg & mask)
    {
        statusReg &= ~mask;
        write_rtc_byte(RTC_STATUS, statusReg);
        return true;
    }
    else
    {
        return false;
    }
}


/**
 * Enable or disable the square wave output.
 * Use a value from the SQWAVE_FREQS_t enumeration for the parameter.
 */
void set_rtc_squareWave(enum SQWAVE_FREQS_t freq)
{
    uint8_t controlReg;

    read_rtc_byte(RTC_CONTROL, &controlReg);
    if (freq >= SQWAVE_NONE)
    {
        controlReg |= _BV(INTCN);
    }
    else
    {
        controlReg = (controlReg & 0xE3) | (freq << RS1);
    }
    write_rtc_byte(RTC_CONTROL, controlReg);
}


/**
 * Returns the value of the oscillator stop flag (OSF) bit in the
 * control/status register which indicates that the oscillator is or
 * was stopped, and that the timekeeping data may be invalid.
 * Optionally clears the OSF bit depending on the argument passed.
 */
bool get_rtc_osc_stopped(bool clearOSF)
{
    uint8_t s;
    
    read_rtc_byte(RTC_STATUS, &s);    		// read the status register
    bool ret = s & _BV(OSF);            // isolate the osc stop flag to return to caller
    if (ret && clearOSF)                // clear OSF if it's set and the caller wants to clear it
    {
        write_rtc_byte( RTC_STATUS, s & ~_BV(OSF) );
    }
    return ret;
}


/**
 * Returns the temperature in Celsius times four.
 */
int16_t get_rtc_temperature()
{
	uint8_t b[2];

    read_rtc_byte(RTC_TEMP_LSB, &b[0]);
    read_rtc_byte(RTC_TEMP_MSB, &b[1]);
    return (((b[1]<<8) | b[0]) / 64);
}



//
// RTC Internal Functions
//

/**
 * Decimal-to-BCD conversion
 */
static uint8_t dec2bcd(uint8_t n)
{
    return n + 6 * (n / 10);
}


/**
 * BCD-to-Decimal conversion
 */
static uint8_t bcd2dec(uint8_t n)
{
    return n - 6 * (n >> 4);
}


/**
 * Break the given time_t into time components.
 * This is a more compact version of the C library localtime function.
 * Note that year is offset from 1970.
 */
void rtc_breakTime(time_t timeInput, tmElements_t* tm){
	uint8_t year;
	uint8_t month, monthLength;
	uint32_t time;
	unsigned long days;

	time = (uint32_t)timeInput;
	tm->Second = time % 60;
	time /= 60; // now it is minutes
	tm->Minute = time % 60;
	time /= 60; // now it is hours
	tm->Hour = time % 24;
	time /= 24; // now it is days
	tm->Wday = ((time + 4) % 7) + 1;  // Sunday is day 1 
  
	year = 0;  
	days = 0;
	while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
		year++;
	}
	tm->Year = year; // year is offset from 1970 
  
	days -= LEAP_YEAR(year) ? 366 : 365;
	time  -= days; // now it is days in this year, starting at 0
  
	days=0;
	month=0;
	monthLength=0;
	for (month=0; month<12; month++) {
		if (month==1) { // february
			if (LEAP_YEAR(year)) {
				monthLength=29;
			} else {
				monthLength=28;
			}
		} else {
			monthLength = monthDays[month];
		}
    
		if (time >= monthLength) {
			time -= monthLength;
		} else {
			break;
		}
	}
	tm->Month = month + 1;  // jan is month 1  
	tm->Day = time + 1;     // day of month
}


/**
 * Assemble time elements into time_t seconds.
 * Note year argument is offset from 1970
 */
time_t rtc_makeTime(const tmElements_t tm){  
	int i;
	uint32_t seconds;

	// seconds from 1970 till 1 jan 00:00:00 of the given year
	seconds= tm.Year*(SECS_PER_DAY * 365);
	for (i = 0; i < tm.Year; i++) {
		if (LEAP_YEAR(i)) {
			seconds +=  SECS_PER_DAY;   // add extra days for leap years
		}
	}
  
	// add days for this year, months start from 1
	for (i = 1; i < tm.Month; i++) {
		if ( (i == 2) && LEAP_YEAR(tm.Year)) { 
			seconds += SECS_PER_DAY * 29;
		} else {
			seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
		}
	}
	seconds+= (tm.Day-1) * SECS_PER_DAY;
	seconds+= tm.Hour * SECS_PER_HOUR;
	seconds+= tm.Minute * SECS_PER_MIN;
	seconds+= tm.Second;
	return (time_t)seconds; 
}