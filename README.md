# Experimental Evaluation
This repository stores all the experiments code. For making the experiments accurate, we proposed a possible real system requirement list detailed as follows.

## Hypothetical System
This section lists the system's requirements used in *Section - Experimental Evaluation*. We provided an example of code that explores different aspects of embedded systems coding. Let us consider the systems has three sensors, network access, Flash access, and has to monitor and make some calculation with the value of the sensors. The following sections detail the features needed. 

### Requirement - R01
It should capture samples of 3 different sensors A (1 byte), B (1 byte) and C (4 bytes) every three seconds;

### Requirement  - R02
It must store, in RAM (volatile), the last ten sensors' samples;

### Requirement  - R03
    It can receive remote requests of six different types:
1. **0xa0** - It requests Sensor A data;
1. **0xa1** - It requests Sensor B data;
1. **0xa2** - It requests Sensor C data;
1. **0xa3** - It requests last ten samples Sensor A data mean;
1. **0xa4** - It requests last ten samples Sensor B data mean;
1. **0xa5** - It requests last ten samples Sensor C data mean;
    
### Requirement - R04
It must store, in Flash (non-volatile), the last sample of sensor A and the last remote response data every 30 seconds. When the system starts, it must recover the flash data;

### Requirement - R05
The system receives a random remote request every 30 seconds.
