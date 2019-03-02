#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <lcd.h>
#include <sqlite3.h>
#include <time.h>

static int callback(void * pNotUsed, int ArgCount, char ** pArgs, char **azColName){
  int Index;
  for (Index=0; Index < ArgCount; Index++){
    printf("%s = %s\n", azColName[Index], pArgs[Index] ? pArgs[Index] : "NULL");
  }
  printf("\n");
  return 0;
}

int main (void) {
    DIR * dir;
    struct dirent * dirent;
    char dev[3][16];   // Dev ID
    char devPath[128]; // Path to device
    char buf[256];     // Data from device
    char tmpData[6];   // Temp C * 1000 reported by device
    char path[] = "/sys/bus/w1/devices";
    ssize_t numRead;
    int lcdfd = -1;
    int devno = 0;
    int maxno = 0;
    float tempC[3];
    float tempF[3];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    int fan1;
    int fan2;
    int count = 0;
    wiringPiSetup();
    lcdfd = lcdInit(2, 16, 4,
                    2,3,
                    6,5,4,1,0,0,0,0);
    lcdClear(lcdfd);
    pinMode (10, OUTPUT);
    pinMode (11, OUTPUT);
    dir = opendir (path);
    rc = sqlite3_open("/home/pi/src/keezerpi/templog.db", &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    }
    if (dir != NULL)
    {
        while ((dirent = readdir (dir)))
        {
            // 1-wire devices are links beginning with 28-
            if (dirent->d_type == DT_LNK && strstr(dirent->d_name, "28-") != NULL)
            {
                strcpy(dev[devno], dirent->d_name);
                printf("\nDevice: %s\n", dev[devno]);
                devno++;
            }
        }
        (void) closedir (dir);
    }
    else
    {
        perror ("Couldn't open the w1 devices directory");
        //return 1;
    }
    maxno = devno;
    // Assemble path to OneWire device
    devno = 0;
    // Read temp continuously
    // Opening the device's file triggers new reading
    while(1)
    {
        int fd = -1;
        sprintf(devPath, "%s/%s/w1_slave", path, dev[devno]);
        fd = open(devPath, O_RDONLY);
        if (fd != -1)
        {
            numRead = read(fd, buf, 256);
            strncpy(tmpData, strstr(buf, "t=") + 2, 5);
            tempC[devno] = strtof(tmpData, NULL);
            tempF[devno] = (tempC[devno] / 1000) * 9 / 5 + 32;
        }
        if (fd != -1)
            close(fd);
        else
            sleep(1);
        devno++;
        lcdPosition (lcdfd, 0, 0) ;
        lcdPrintf(lcdfd, " %.1f %.1f %.1f ", tempF[0], tempF[1], tempF[2]);
        lcdPosition(lcdfd, 0, 1);
        if (count / 6  % 4 == 0)
        {
            lcdPrintf(lcdfd, "1. Home Brew ");
        }
        else if (count / 6 % 4 == 1)
        {
            lcdPrintf(lcdfd, "2.              ");
        }
        else if (count / 6 %4 == 2)
        {
            lcdPrintf(lcdfd, "3. Hazy Ltl Thg ");
        }
        else
        {
            lcdPrintf(lcdfd, "4. Dragon's Milk");
        }
        if (tempF[0] > 52)
        {
            fan1 = 100;
            fan2 = 100;
            digitalWrite(10, 1);
            digitalWrite(11, 1);
        }

        if (tempF[0] < 51)
        {
            fan1 = 0;
            fan2 = 0;
            digitalWrite(10, 0);
            digitalWrite(11, 0);
        }
        if (devno >= maxno)
        {
            devno=0;
        }
        if (count++ > 100)
        {
            char data[255];
            sprintf(data, "INSERT INTO temps (timestamp, temp1, temp2, temp3, fan1, fan2) VALUES (%lu, %f, %f, %f, %d, %d)",
                    clock(), tempF[0], tempF[1], tempF[2], fan1, fan2);
            printf("%s\n", data);
            rc = sqlite3_exec(db, data, callback, 0, &zErrMsg);
            if (rc != SQLITE_OK)
            {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
            lcdReset(lcdfd);
            count = 0;
        }
    }
/* return 0; --never called due to loop */
}

