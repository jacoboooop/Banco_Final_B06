#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "comun.h"

//prueba de ommit
Config leer_configuracion(const char *ruta){
    FILE *archivo = fopen(ruta, "r");

    //Si el archivo no se abre bien
    if (archivo == NULL){
        perror("Error al abrir config.txt");
        exit(1);
    }

    //Cuando se abre bien al archivo
    Config config;
    char linea[100];
    while (fgets(linea, sizeof(linea), archivo)) {
        if (linea[0] == '#' || strlen(linea) < 3) continue; // Ignorar comentarios y líneas vacías
        if (strstr(linea, "LIMITE_RETIRO")) sscanf(linea, "LIMITE_RETIRO=%d", &config.limite_retiro);
        else if (strstr(linea, "LIMITE_TRANSFERENCIA")) sscanf(linea, "LIMITE_TRANSFERENCIA=%d", &config.limite_transferencia);
        else if (strstr(linea, "UMBRAL_RETIROS")) sscanf(linea, "UMBRAL_RETIROS=%d",&config.umbral_retiros);
        else if (strstr(linea, "UMBRAL_TRANSFERENCIAS")) sscanf(linea,"UMBRAL_TRANSFERENCIAS=%d", &config.umbral_transferencias);
        else if (strstr(linea, "NUM_HILOS")) sscanf(linea, "NUM_HILOS=%d",&config.num_hilos);
        else if (strstr(linea, "ARCHIVO_CUENTAS")) sscanf(linea, "ARCHIVO_CUENTAS=%s",config.archivo_cuentas);
        else if (strstr(linea, "ARCHIVO_LOG")) sscanf(linea, "ARCHIVO_LOG=%s",config.archivo_log);
    }
    fclose(archivo);
    return config;
}

void AgregarLog(const char *operacion) {

    
    Config configuracion = leer_configuracion("../Archivos_datos/config.txt");

    FILE *archivoLog = fopen(configuracion.archivo_log, "a");
    
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char hora_actual[20];
    strftime(hora_actual, sizeof(hora_actual), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(archivoLog, "%s ---- %s\n", hora_actual, operacion);
    fclose(archivoLog);

}

void* Estado_banco(void* arg) {
    pid_t pid_banco = *(pid_t *)arg;
    char comando[200];
    while(1){
        if (kill(pid_banco, 0) == -1){
            snprintf(comando, sizeof(comando), "kill -9 %d", getpid());
            system(comando);
        }
        sleep(4);
    }

    return NULL;
}