#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

// CAN Message IDs
#define PDM1_MESSAGE      0x14EF111E  // PDM1 status
#define PDM2_MESSAGE      0x14EF111F  // PDM2 status
#define PDM1_SHORT        0x14E9111E  // PDM1 short message
#define PDM2_SHORT        0x14E9111F  // PDM2 short message
#define PDM1_COMMAND      0x14EF1E11  // PDM1 command message
#define PDM2_COMMAND      0x14EF1F11  // PDM2 command message
#define RIXENS_COMMAND    0x788       // Rixen control command
#define RIXENS_RETURN1    0x78A       // Rixen response 1
#define RIXENS_RETURN2    0x78B       // Rixen response 2
#define RIXENS_RETURN3    0x724       // Heat source selection
#define RIXENS_RETURN4    0x725       // Fan speed, fuel usage
#define RIXENS_GLYCOL     0x726       // Glycol temp & voltage
#define RIXENS_RETURN6    0x728       // Rixen response 6
#define THERMOSTAT_AMBIENT_STATUS 0x19FF9C58  // Cabin temperature (extended ID)
#define TANK_LEVEL        0x19FFB7AF  // Tank level (extended ID)

#endif
