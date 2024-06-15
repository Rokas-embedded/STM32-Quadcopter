#include "./bn357.h"

UART_HandleTypeDef *uart;

volatile float m_latitude = 0.0;
volatile float m_longitude = 0.0;
volatile float m_altitude = 0.0;
volatile float m_altitude_geoid = 0.0;
volatile float m_accuracy = 0.0;
volatile uint8_t m_satellites_quantity = 0.0;
volatile uint8_t m_fix_quality = 0;
volatile uint8_t m_time_utc_hours = 0;
volatile uint8_t m_time_utc_minutes = 0;
volatile uint8_t m_time_utc_seconds = 0;
volatile uint8_t m_up_to_date_date = 0;
volatile uint32_t m_time_raw = 0;


#define BN357_DEBUG 0

volatile uint8_t m_logging = 0;

// consider adding mutexes even though interrupt always finished everything before letting the cpu continue reading

uint8_t init_bn357(UART_HandleTypeDef *uart_temp, uint8_t logging){
    uart = uart_temp;
    m_logging = logging;
    return 1;
}

// Check the status of the gps reading. If they are up to date or not
uint8_t bn357_get_status_up_to_date(uint8_t reset_afterwards){
    uint8_t temp_status = m_up_to_date_date;
    if(reset_afterwards){
        m_up_to_date_date = 0;
    }
    return temp_status;
}

void bn357_get_clear_status(){
    m_up_to_date_date = 0;
}

uint8_t bn357_parse_and_store(unsigned char *gps_output_buffer, uint16_t size_of_buffer){
    // reset the state of successful gps parse
    m_up_to_date_date = 0;
    // find the starting position of the substring
    char* start = strstr(gps_output_buffer, "$GNGGA");  
    // Check if the start exists
    if(start == NULL){
#if(BN357_DEBUG)
        printf("Failed at Check if the start exists\n");
#endif
        return 0;
    }
    char* end = strchr(start, '\n');  // find the position of the first new line character after the substring
    // check if the end of it exists
    if (end == NULL){
#if(BN357_DEBUG)
        printf("Failed at check if the end of it exists\n");
#endif
        return 0;
    }
    int length = end - start;  // calculate the length of the substring
    char sub_string[length + 1];  // create a new string to store the substring, plus one for null terminator
    strncpy(sub_string, start, length);  // copy the substring to the new string
    sub_string[length] = '\0';  // add a null terminator to the new string
#if(BN357_DEBUG)
    printf("Substring: %s\n", sub_string);  // print the substring
#endif

    // find utc time
    start = strchr(sub_string, ',') + 1;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at time\n");
#endif
        return 0;
    }
    end = strchr(start, ',');
    length = end - start;
    char utc_time_string[length + 1];
    strncpy(utc_time_string, start, length);
    utc_time_string[length] = '\0';
    uint32_t time = atoi(utc_time_string);
    m_time_utc_seconds = time % 100;
    m_time_utc_minutes = ((time % 10000) - m_time_utc_seconds) / 100;
    m_time_utc_hours = ((time % 1000000) - (m_time_utc_minutes * 100) - m_time_utc_seconds) / 10000;
#if(BN357_DEBUG)
    printf("UTC time: %d%d%d%\n", m_time_utc_hours, m_time_utc_minutes, m_time_utc_seconds);
#endif

    // find latitude
    start = end+1;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at latitude\n");
#endif
        return 0;
    }
    end = strchr(start, ',');
    length = end - start;
    char latitude_string[length + 1];
    strncpy(latitude_string, start, length);
    latitude_string[length] = '\0';
    uint16_t latitude_temp = atoi(latitude_string);
    uint16_t latitude_degrees = ((latitude_temp % 10000) - (latitude_temp % 100)) / 100;
    m_latitude = (float)latitude_degrees + ((atof(latitude_string) - (float)latitude_degrees * 100)/60);
#if(BN357_DEBUG)
    printf("Latitude: %f\n", m_latitude);
#endif

    // find longitude
    start = end+3; // skip the N character
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at longitude\n");
#endif
        return 0;
    }
    end = strchr(start, ',');
    length = end - start;
    char longitude_string[length + 1];
    strncpy(longitude_string, start, length);
    longitude_string[length] = '\0';
    uint16_t longitude_temp = atoi(longitude_string);
    uint16_t longitude_degrees = ((longitude_temp % 10000) - (longitude_temp % 100)) / 100;
    m_longitude = (float)longitude_degrees + ((atof(longitude_string) - (float)longitude_degrees * 100)/60);
#if(BN357_DEBUG)
    printf("Longitude: %f\n", m_longitude);
#endif

    // find fix quality
    start = end+3; // skip the E character
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at fix quality\n");
#endif
        return 0;
    }
    end = strchr(start, ',');
    length = end - start;
    char fix_qualtiy_string[length + 1];
    strncpy(fix_qualtiy_string, start, length);
    fix_qualtiy_string[length] = '\0';
    m_fix_quality = atoi(fix_qualtiy_string);
#if(BN357_DEBUG)
    printf("Fix quality: %d\n", m_fix_quality);
#endif

    // find satellites quantity
    start = end+1;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at satellites quantity\n");
#endif
        return 0;
    }
    end = strchr(start, ',');
    length = end - start;
    char satellites_quantity_string[length + 1];
    strncpy(satellites_quantity_string, start, length);
    satellites_quantity_string[length] = '\0';
    m_satellites_quantity = atoi(satellites_quantity_string);
#if(BN357_DEBUG)
    printf("Satellites quantity: %d\n", m_satellites_quantity);
#endif

    // find accuracy
    start = end+1;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at accuracy\n");
#endif
        return 0;
    } 
    end = strchr(start, ',');
    length = end - start;
    char accuracy_quantity_string[length + 1];
    strncpy(accuracy_quantity_string, start, length);
    accuracy_quantity_string[length] = '\0';
    m_accuracy = atof(accuracy_quantity_string);
#if(BN357_DEBUG)
    printf("Accuracy: %f\n", m_accuracy);
#endif

    // find altitude above sea levels
    start = end+1;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at altitude\n");
#endif
        return 0;
    } 
    end = strchr(start, ',');
    length = end - start;
    char altitude_string[length + 1];
    strncpy(altitude_string, start, length);
    altitude_string[length] = '\0';
    m_altitude = atof(altitude_string);
#if(BN357_DEBUG)
    printf("Altitude: %f\n", m_altitude);
#endif

    // find geoid altitude deviation
    start = end+3;
    if(start[0] == ','){
#if(BN357_DEBUG)
        printf("Failed at Geoid altitude\n");
#endif
        return 0;
    } 
    end = strchr(start, ',');
    length = end - start;
    char geoid_altitude_string[length + 1];
    strncpy(geoid_altitude_string, start, length);
    geoid_altitude_string[length] = '\0';
    m_altitude_geoid = atof(geoid_altitude_string);
#if(BN357_DEBUG)
    printf("Geoid altitude: %f\n", m_altitude_geoid);
#endif

    m_up_to_date_date = 1;
#if(BN357_DEBUG)
    printf("GPS data read successful\n");
#endif
    // Might be interested in the speed and ground course values in the other info the gps gives

    return 1;
}

float bn357_get_latitude_decimal_format(){
    return m_latitude;
}

float bn357_get_longitude_decimal_format(){
    return m_longitude;
}

float bn357_get_altitude_meters(){
    return m_altitude;
}

float bn357_get_geoid_altitude_meters(){
    return m_altitude_geoid;
}

float bn357_get_accuracy(){
    return m_accuracy;
}

uint8_t bn357_get_satellites_quantity(){
    return m_satellites_quantity;
}

uint8_t bn357_get_fix_quality(){
    return m_fix_quality;
}

uint8_t bn357_get_utc_time_hours(){
    return m_time_utc_hours;
}

uint8_t bn357_get_utc_time_minutes(){
    return m_time_utc_minutes;
}

uint8_t bn357_get_utc_time_seconds(){
    return m_time_utc_seconds;
}

uint8_t bn357_get_utc_time_raw(){
    return m_time_raw;
}