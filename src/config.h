const char* ssid     = "your wifi ssid";
const char* password = "your wifi password";

const char* Timezone    = "CST6CDT,M3.2.0,M11.1.0";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 

// un-comment this line to use Metric instead of Imperial
//#define USE_METRIC
                          
String apikey  = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    // Use your own API key by signing up for a free developer account at https://openweathermap.org/
String lang    = "en";                                  // Language: https://openweathermap.org/api/one-call-api#multi
String lat     = "40.666666";                           // latitude for OpenWeatherMap API call
String lon     = "-89.691636";                          // longitude for OpenWeatherMap API call


// if you'd like to override the literal strings, do so here
// note: if you use characters not used in the English language, you might need to update the font files
const String TXT_SUNRISE           = "Sunrise";
const String TXT_SUNSET            = "Sunset";
const String TXT_CURRENT           = "Right Now";
const String TXT_TEMPERATURE       = "Temperature";
const String TXT_FEELS_LIKE        = "Feels Like";
const String TXT_HUMIDITY          = "RH";            // abbreviation for relative humidity
const String TXT_UPDATED           = "Updated";       // "Updated" part of "Updated at" time
const String TXT_AT                = "at";            // "at" part of "Updated at" time

// UTC Unix timestamp of the date of a previous New Moon
// Get a date/time here: https://www.timeanddate.com/moon/phases/
// And convert it to UTC Unix timestamp here: https://www.epochconverter.com/
int new_moon = 1610514000;


// no need to touch this, the pre-processor will switch these automatically if you un-commented "USE_METRIC" at the top
#ifdef USE_METRIC
String units             = "metric";
const String TXT_DEGREE  = "°C";  // Celsius
const String TXT_SPEED   = "MPS";  // meters per second
#else
String units             = "imperial";
const String TXT_DEGREE  = "°F";  // Fahrenheit
const String TXT_SPEED   = "MPH";  // miles per hour
#endif
