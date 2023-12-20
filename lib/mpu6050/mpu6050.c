#include "./mpu6050.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#define MPU6050 (0b1101000 << 1)
#define I2C_MASTER_TIMEOUT_MS 1000
#define ID_REG 0x75
#define ID_VALUE 104
#define PWR_MGMT_REG 0x6B
#define ACCEL_XOUT_H_REG 0x3B
#define GYRO_XOUT_H_REG 0x43

volatile float m_accelerometer_correction[3] = {
    0, 0, 0};

volatile float m_gyro_correction[3] = {
    0, 0, 0};

volatile int64_t m_previous_time = 0;
volatile float m_complementary_ratio = 0.0;

I2C_HandleTypeDef *i2c_handle;

uint8_t init_mpu6050(I2C_HandleTypeDef *i2c_handle_temp, uint8_t apply_calibration, float accelerometer_correction[3], float gyro_correction[3], float refresh_rate_hz, float complementary_ratio)
{
    i2c_handle = i2c_handle_temp;

    if (apply_calibration)
    {
        // assign the correction for gyro
        for (uint8_t i = 0; i < 3; i++)
        {
            m_gyro_correction[i] = gyro_correction[i];
        }

        // assign the correction for accelerometer
        for (uint8_t i = 0; i < 3; i++)
        {
            m_accelerometer_correction[i] = accelerometer_correction[i];
        }
    }

    m_complementary_ratio = complementary_ratio;

    uint8_t check;
    HAL_I2C_Mem_Read(i2c_handle, MPU6050 + 1, ID_REG, 1, &check, 1, 100);

    if (check != ID_VALUE)
    {
        printf("MPU6050 initialization failed\n");
        return 0;
    }

    uint8_t reset_device1 = 0b00000000;
    reset_device1 |= PWR_RESET;
    reset_device1 |= PWR_TEMP_DIS;
    reset_device1 |= PWR_CLOCK_INTERNAL_8MHZ;

    HAL_I2C_Mem_Write(
        i2c_handle, 
        MPU6050, 
        PWR_MGMT_REG, 
        1, 
        &reset_device1, 
        1, 
        100);
    HAL_Delay(100);

    uint8_t reset_device2 = 0b00000000;
    reset_device2 |= PWR_CLOCK_INTERNAL_STOP;

    HAL_I2C_Mem_Write(
        i2c_handle, 
        MPU6050, 
        PWR_MGMT_REG, 
        1, 
        &reset_device2, 
        1, 
        100);
    HAL_Delay(100);

    uint8_t reset_device3 = 0b00000000;
    reset_device3 |= PWR_TEMP_DIS;
    reset_device3 |= PWR_CLOCK_INTERNAL_8MHZ;

    HAL_I2C_Mem_Write(
        i2c_handle, 
        MPU6050, 
        PWR_MGMT_REG, 
        1, 
        &reset_device3, 
        1, 
        100);

    printf("MPU6050 initialized\n");
    return 1;
}
// Read accelerometer in gravity units
void mpu6050_get_accelerometer_readings_gravity(float *data)
{
    uint8_t retrieved_data[] = {0, 0, 0, 0, 0, 0};

    HAL_I2C_Mem_Read(
        i2c_handle,
        MPU6050 + 1,
        ACCEL_XOUT_H_REG,
        1,
        retrieved_data,
        6,  // Read all of the accelerometer registers
        100);

    int16_t X = ((int16_t)retrieved_data[0] << 8) | (int16_t)retrieved_data[1];
    int16_t Y = ((int16_t)retrieved_data[2] << 8) | (int16_t)retrieved_data[3];
    int16_t Z = ((int16_t)retrieved_data[4] << 8) | (int16_t)retrieved_data[5];

    float X_out = ((float)X) / 16384.0;
    float Y_out = ((float)Y) / 16384.0;
    float Z_out = ((float)Z) / 16384.0;

    data[0] = X_out - (m_accelerometer_correction[0]);
    data[1] = Y_out - (m_accelerometer_correction[1]);
    data[2] = Z_out - (m_accelerometer_correction[2] - 1);
}

// Read gyro in degrees per second units 
void mpu6050_get_gyro_readings_dps(float *data)
{
    uint8_t retrieved_data[] = {0, 0, 0, 0, 0, 0};

    HAL_I2C_Mem_Read(
        i2c_handle,
        MPU6050 + 1,
        GYRO_XOUT_H_REG,
        1,
        retrieved_data,
        6,  // Read all of the gyroscope registers
        100);

    int16_t X = ((int16_t)retrieved_data[0] << 8) | (int16_t)retrieved_data[1];
    int16_t Y = ((int16_t)retrieved_data[2] << 8) | (int16_t)retrieved_data[3];
    int16_t Z = ((int16_t)retrieved_data[4] << 8) | (int16_t)retrieved_data[5];

    float X_out = ((float)X) / 131.0;
    float Y_out = ((float)Y) / 131.0;
    float Z_out = ((float)Z) / 131.0;

    data[0] = X_out - (m_gyro_correction[0]);
    data[1] = Y_out - (m_gyro_correction[1]);
    data[2] = Z_out - (m_gyro_correction[2]);
}

void calculate_pitch_and_roll(float *data, float *roll, float *pitch)
{
    float x = data[0];
    float y = data[1];
    float z = data[2];

    float acc_vector_length = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
    x = x / acc_vector_length;
    y = y / acc_vector_length;
    z = z / acc_vector_length;

    // rotation around the x axis
    *roll = atan2f(y, z) * (180 / M_PI);

    // rotation around the y axis
    *pitch = -(asinf(x) * (180 / M_PI));
    // i put a minus on the pitch calculation as i have found that the pitch has opposite values of actual
}

// Get the x and y degrees from accelerometer.
void calculate_degrees_x_y(float *data, float *rotation_around_x, float *rotation_around_y)
{
    float x = data[0];
    float y = data[1];
    float z = data[2];

    *rotation_around_x = atan2f(y, sqrtf(x * x + z * z)) * (180.0 / M_PI);
    // added minus to match actual dps direction the values are supposed to go
    *rotation_around_y = -(atan2f(x, sqrtf(y * y + z * z)) * (180.0 / M_PI));
}

// Get many values of the accelerometer error and average them together. Then print out the result
void find_accelerometer_error(uint64_t sample_size)
{
    float x_sum = 0, y_sum = 0, z_sum = 0;
    float data[] = {0, 0, 0};

    float accelerometer_correction_temp[3];
    float gyro_correction_temp[3];

    // Copy and delete the original corrections
    for (uint8_t i = 0; i < 3; i++){
        gyro_correction_temp[i] = m_gyro_correction[i];
        m_gyro_correction[i] = 0;

        accelerometer_correction_temp[i] = m_accelerometer_correction[i];
        m_accelerometer_correction[i] = 0;
    }

    for (uint64_t i = 0; i < sample_size; i++)
    {
        mpu6050_get_accelerometer_readings_gravity(data);
        x_sum += data[0];
        y_sum += data[1];
        z_sum += data[2];
        // It can sample stuff at 1KHz
        // but 0.5Khz is just to be safe
        HAL_Delay(2);
    }

    // Reassign the corrections 
    for (uint8_t i = 0; i < 3; i++){
        m_gyro_correction[i] = gyro_correction_temp[i];
        m_accelerometer_correction[i] = accelerometer_correction_temp[i];
    }

    printf(
        "ACCELEROMETER errors:  X,Y,Z  %f, %f, %f\n",
        x_sum / sample_size,
        y_sum / sample_size,
        (z_sum / sample_size) -1 );
}

// Get many values of the gyro error and average them together. Then print out the result
void find_gyro_error(uint64_t sample_size)
{

    float x_sum = 0, y_sum = 0, z_sum = 0;
    float data[] = {0, 0, 0};

    float accelerometer_correction_temp[3];
    float gyro_correction_temp[3];

    // Copy and delete the original corrections
    for (uint8_t i = 0; i < 3; i++){
        gyro_correction_temp[i] = m_gyro_correction[i];
        m_gyro_correction[i] = 0;

        accelerometer_correction_temp[i] = m_accelerometer_correction[i];
        m_accelerometer_correction[i] = 0;
    }

    for (uint64_t i = 0; i < sample_size; i++)
    {
        mpu6050_get_gyro_readings_dps(data);

        x_sum += data[0];
        y_sum += data[1];
        z_sum += data[2];
        // It can sample stuff at 1KHz
        // but 0.5Khz is just to be safe
        HAL_Delay(2);
    }

    // Reassign the corrections 
    for (uint8_t i = 0; i < 3; i++){
        m_gyro_correction[i] = gyro_correction_temp[i];
        m_accelerometer_correction[i] = accelerometer_correction_temp[i];
    }

    printf(
        "GYRO errors:  X,Y,Z  %f, %f, %f\n",
        x_sum / sample_size,
        y_sum / sample_size,
        z_sum / sample_size);
}

void find_and_return_gyro_error(uint64_t sample_size, float *return_array)
{

    float x_sum = 0, y_sum = 0, z_sum = 0;
    float data[] = {0, 0, 0};

    float accelerometer_correction_temp[3];
    float gyro_correction_temp[3];

    // Copy and delete the original corrections
    for (uint8_t i = 0; i < 3; i++){
        gyro_correction_temp[i] = m_gyro_correction[i];
        m_gyro_correction[i] = 0;

        accelerometer_correction_temp[i] = m_accelerometer_correction[i];
        m_accelerometer_correction[i] = 0;
    }

    for (uint64_t i = 0; i < sample_size; i++)
    {
        mpu6050_get_gyro_readings_dps(data);

        x_sum += data[0];
        y_sum += data[1];
        z_sum += data[2];
        // It can sample stuff at 1KHz
        // but 0.5Khz is just to be safe
        HAL_Delay(2);
    }

    // Reassign the corrections 
    for (uint8_t i = 0; i < 3; i++){
        m_gyro_correction[i] = gyro_correction_temp[i];
        m_accelerometer_correction[i] = accelerometer_correction_temp[i];
    }

    printf(
        "GYRO errors:  X,Y,Z  %f, %f, %f\n",
        x_sum / sample_size,
        y_sum / sample_size,
        z_sum / sample_size);

    return_array[0] = x_sum / sample_size;
    return_array[1] = y_sum / sample_size;
    return_array[2] = z_sum / sample_size;
}

void mpu6050_apply_calibrations(float accelerometer_correction[3], float gyro_correction[3]){
    // assign the correction for gyro
    for (uint8_t i = 0; i < 3; i++)
    {
        m_gyro_correction[i] = gyro_correction[i];
    }

    // assign the correction for accelerometer
    for (uint8_t i = 0; i < 3; i++)
    {
        m_accelerometer_correction[i] = accelerometer_correction[i];
    }
}


// Do complementary filter for x(pitch) and y(roll) and z(yaw). Combine accelerometer and gyro to get a more usable gyro value. Please make sure the coefficient is scaled by refresh rate. It helps a lot.
void convert_angular_rotation_to_degrees(float* gyro_angular, float* gyro_degrees, float rotation_around_x, float rotation_around_y, float rotation_around_z, int64_t time){
    if(m_previous_time == 0){
        m_previous_time = time;
        return;
    }

    double elapsed_time_sec= (((double)time/1000.0)-((double)m_previous_time/1000.0));
    m_previous_time = time;

    // Convert degrees per second and add the complementary filter with accelerometer degrees
    gyro_degrees[0] = (1.0 - m_complementary_ratio) * (gyro_degrees[0] + gyro_angular[0] * elapsed_time_sec) + m_complementary_ratio * rotation_around_x;
    gyro_degrees[1] = (1.0 - m_complementary_ratio) * (gyro_degrees[1] + gyro_angular[1] * elapsed_time_sec) + m_complementary_ratio * rotation_around_y;
    gyro_degrees[2] = (1.0 - m_complementary_ratio) * (gyro_degrees[2] + gyro_angular[2] * elapsed_time_sec) + m_complementary_ratio * rotation_around_z;

    // I dont want to track how many times the degrees went over the 360 degree mark, no point.
    while (gyro_degrees[0] > 180.0) {
        gyro_degrees[0] -= 360.0;
    }
    while (gyro_degrees[0] < -180.0) {
        gyro_degrees[0] += 360.0;
    }

    while (gyro_degrees[1] > 180.0) {
        gyro_degrees[1] -= 360.0;
    }
    while (gyro_degrees[1] < -180.0) {
        gyro_degrees[1] += 360.0;
    }

    while (gyro_degrees[2] > 180.0) {
        gyro_degrees[2] -= 360.0;
    }
    while (gyro_degrees[2] < -180.0) {
        gyro_degrees[2] += 360.0;
    }
}

// Do complementary filter for x(pitch) and y(roll). Combine accelerometer and gyro to get a more usable gyro value. Please make sure the coefficient is scaled by refresh rate. It helps a lot.
void convert_angular_rotation_to_degrees_x_y(float* gyro_angular, float* gyro_degrees, float rotation_around_x, float rotation_around_y, int64_t time, uint8_t set_timestamp){

    if(m_previous_time == 0){
        m_previous_time = time;
        return;
    }

    double elapsed_time_sec= (((double)time/1000.0)-((double)m_previous_time/1000.0));
    if(set_timestamp == 1){
        m_previous_time = time;
    }

    // Convert degrees per second and add the complementary filter with accelerometer degrees
    gyro_degrees[0] = (1.0 - m_complementary_ratio) * (gyro_degrees[0] + gyro_angular[0] * elapsed_time_sec) + m_complementary_ratio * rotation_around_x;
    gyro_degrees[1] = (1.0 - m_complementary_ratio) * (gyro_degrees[1] + gyro_angular[1] * elapsed_time_sec) + m_complementary_ratio * rotation_around_y;

    // I dont want to track how many times the degrees went over the 360 degree mark, no point.
    while (gyro_degrees[0] > 180.0) {
        gyro_degrees[0] -= 360.0;
    }
    while (gyro_degrees[0] < -180.0) {
        gyro_degrees[0] += 360.0;
    }

    while (gyro_degrees[1] > 180.0) {
        gyro_degrees[1] -= 360.0;
    }
    while (gyro_degrees[1] < -180.0) {
        gyro_degrees[1] += 360.0;
    }
}

// Find the shortest value between two angles. Range -180 to 180
float angle_difference(float a, float b) {

    /// How to understand this
    //  So -177 % 179 = 2
    //  2 - 180 = -178
    // -178 < -180 False
    // -178 returned

    // If it moves in the same direction over time it will be -178, -179, -180, 180, 181

    //  So 177 % -179 = -2
    //  - 2 - 180 = -182
    // -182 < -180 True
    // -182+360 = 178 returned

    // fmodf is modulus operator (%) for floats
    float diff = fmodf(b - a + 180, 360) - 180;
    return diff < -180 ? diff + 360 : diff;
}

// Do complementary filter to merge magnetometer and gyro values
void convert_angular_rotation_to_degrees_z(float* gyro_angular, float* gyro_degrees, float rotation_around_z, int64_t time){
    // Convert angular velocity to actual degrees that it moved and add it to the integral (dead reckoning not PID)

    if(m_previous_time == 0){
        m_previous_time = time;
        return;
    }

    double elapsed_time_sec = (((double)time/1000.0)-((double)m_previous_time/1000.0));
    m_previous_time = time;

    // Gyro without magnetometer
    float gyro_integration = gyro_degrees[2] + gyro_angular[2] * elapsed_time_sec;

    // Use the angle_difference function to find the smallest difference between gyro_integration and rotation_around_z
    float angle_diff = angle_difference(gyro_integration, rotation_around_z);

    // Use the angle difference value as the magnetometer in this sensor fusion 

    // Works bad
    // gyro_degrees[2] = (1.0 - m_complementary_ratio) * (gyro_integration) + m_complementary_ratio * angle_diff;

    // Works not as bad, but still bad
    // gyro_degrees[2] = (1.0 - m_complementary_ratio) * gyro_integration + m_complementary_ratio * (gyro_integration + angle_diff);

    // Works very good adding it on top. Not as good as raw magnetometer yaw though
    gyro_degrees[2] = gyro_integration + m_complementary_ratio * angle_diff;

    // I dont want to track how many times the degrees went over the 360 degree mark, no point.
    while (gyro_degrees[2] > 180.0) {
        gyro_degrees[2] -= 360.0;
    }
    while (gyro_degrees[2] < -180.0) {
        gyro_degrees[2] += 360.0;
    }
}