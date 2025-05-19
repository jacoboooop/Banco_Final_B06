#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include "comun.h"

typedef struct
{
    int id;
    char fecha[50];
    char tipo[50];
    float cantidad;
} Operaciones;

void *TransferenciaConsecutivos(void *arg);
void *RetiroConsecutivos(void *arg);
void *RetiroSuperior(void *arg);
void *TransferenciaSuperior(void *arg);
// void* escribirBanco(void *arg);

sem_t *semaforoMonitor;
sem_t *semaforo_log;
Config configuracion;
char Transacciones[] = "../Archivos_datos/Transacciones.log";

int main(int argc, char *argv[])
{

    semaforoMonitor = sem_open("semaforo_monitor", O_CREAT, 0644, 1);
    semaforo_log = sem_open("semaforo_log", 0);
    // semaforoMonitor = sem_open("semaforo_monitor", 0);

    sem_wait(semaforo_log);
    AgregarLog("Se han creado los semaforos de monitor");
    sem_post(semaforo_log);

    pthread_t pthread_TransferenciaConsecutivos, pthread_RetiroConsecutivos, pthread_RetiroSuperior, pthread_TransferenciaSuperior, banco;

    int pid_banco = atoi(argv[1]);
    pthread_create(&banco, NULL, Estado_banco, &pid_banco);

    sem_wait(semaforo_log);
    AgregarLog("");
    sem_post(semaforo_log);

    sem_wait(semaforo_log);
    AgregarLog("Se han lanzado los hilos de lectura de anomalias");
    sem_post(semaforo_log);

    pthread_create(&pthread_TransferenciaConsecutivos, NULL, &TransferenciaConsecutivos, NULL);
    pthread_create(&pthread_RetiroConsecutivos, NULL, &RetiroConsecutivos, NULL);
    pthread_create(&pthread_RetiroSuperior, NULL, &RetiroSuperior, NULL);
    pthread_create(&pthread_TransferenciaSuperior, NULL, &TransferenciaSuperior, NULL);
    pthread_join(pthread_TransferenciaConsecutivos, NULL);
    pthread_join(pthread_RetiroConsecutivos, NULL);
    pthread_join(pthread_RetiroSuperior, NULL);
    pthread_join(pthread_TransferenciaSuperior, NULL);

    sem_close(semaforoMonitor);
    sem_unlink("semaforo_monitor");

    return 0;
}

void *TransferenciaConsecutivos(void *arg)
{
    while (1)
    {
        sem_wait(semaforoMonitor);

        configuracion = leer_configuracion("../Archivos_datos/config.txt");

        // supongo un total de 200 operaciones
        Operaciones operaciones[200];
        int TotalOperaciones = 0;
        char linea[255];

        sem_wait(semaforo_log);
        AgregarLog("Se esta leyendo el documento de transacciones en modo lectura");
        sem_post(semaforo_log);
        // abro el archivo de transacciones
        FILE *archivo = fopen(Transacciones, "r");

        // leo el fichero
        while (fgets(linea, sizeof(linea), archivo) != NULL && TotalOperaciones < 100)
        {
            int id;
            char fecha[50];
            char tipo[50];
            float cantidad;

            // cogo la linea de
            if (sscanf(linea, "%d;%49[^;];%49[^;];%f", &id, fecha, tipo, &cantidad) == 4)
            {
                operaciones[TotalOperaciones].id = id;
                strcpy(operaciones[TotalOperaciones].fecha, fecha);
                strcpy(operaciones[TotalOperaciones].tipo, tipo);
                operaciones[TotalOperaciones].cantidad = cantidad;
                TotalOperaciones++;
            }
        }
        fclose(archivo);

        // segun el numero de operaciones q ha realizado un usuario
        for (int i = 0; i < TotalOperaciones; i++)
        {
            int idActual = operaciones[i].id;
            char fechaActual[50];
            strcpy(fechaActual, operaciones[i].fecha);

            int countTransferencia = 0;

            char tipoCompara[50];
            // comparo el id, la fecha y el tipo de operacion
            for (int j = 0; j < TotalOperaciones; j++)
            {
                if (operaciones[j].id == idActual && strcmp(operaciones[j].fecha, fechaActual) == 0)
                {
                    strcpy(tipoCompara, operaciones[j].tipo);
                    if (strcmp(tipoCompara, "transferencia") == 0)
                    {
                        countTransferencia++;
                    }
                }
            }
            // si supera 5 transacciones
            if (countTransferencia > configuracion.umbral_transferencias)
            {
                sem_wait(semaforo_log);
                AgregarLog("Se ha notificado al banco de la anomalia");
                sem_post(semaforo_log);
                printf("Anomalía detectada, demasiadas transferencias: Usuario %d en fecha %s realizó %d operaciones\n", idActual, fechaActual, countTransferencia);
            }
        }
        sem_post(semaforoMonitor);
    }
    return NULL;
}

void *RetiroConsecutivos(void *arg)
{
    while (1)
    {

        sem_wait(semaforoMonitor);

        configuracion = leer_configuracion("../Archivos_datos/config.txt");

        // supongo un maximo de 200 operaciones
        Operaciones operaciones[200];
        int TotalOperaciones = 0;
        char linea[255];

        sem_wait(semaforo_log);
        AgregarLog("Se esta leyendo el documento de transacciones en modo lectura");
        sem_post(semaforo_log);
        // abro el archivo de transacciones
        FILE *archivo = fopen(Transacciones, "r");

        // leo el fichero
        while (fgets(linea, sizeof(linea), archivo) != NULL && TotalOperaciones < 100)
        {
            int id;
            char fecha[50];
            char tipo[50];
            float cantidad;

            // cogo la linea de
            if (sscanf(linea, "%d;%49[^;];%49[^;];%f", &id, fecha, tipo, &cantidad) == 4)
            {
                operaciones[TotalOperaciones].id = id;
                strcpy(operaciones[TotalOperaciones].fecha, fecha);
                strcpy(operaciones[TotalOperaciones].tipo, tipo);
                operaciones[TotalOperaciones].cantidad = cantidad;
                TotalOperaciones++;
            }
        }
        fclose(archivo);

        // Se agrupa por usuario y fecha, pero solo se cuentan los registros cuyo tipo sea "retiro"
        for (int i = 0; i < TotalOperaciones; i++)
        {
            int idActual = operaciones[i].id;
            char fechaActual[50];
            strcpy(fechaActual, operaciones[i].fecha);

            int countRetiros = 0;
            // Convertimos a minúsculas la cadena tipo para hacer comparaciones insensibles
            char tipoComparar[50];
            for (int j = 0; j < TotalOperaciones; j++)
            {
                if (operaciones[j].id == idActual && strcmp(operaciones[j].fecha, fechaActual) == 0)
                {
                    strcpy(tipoComparar, operaciones[j].tipo);
                    if (strcmp(tipoComparar, "retiro") == 0)
                    {
                        countRetiros++;
                    }
                }
            }
            if (countRetiros > configuracion.umbral_retiros)
            {
                sem_wait(semaforo_log);
                AgregarLog("Se ha notificado al banco de la anomalia");
                sem_post(semaforo_log);
                printf("Anomalía detectada, retiros sospechosos: Usuario %d en fecha %s realizó %d retiros\n", idActual, fechaActual, countRetiros);
            }
        }
        sem_post(semaforoMonitor);
    }
    return NULL;
}

void *RetiroSuperior(void *arg)
{
    while (1)
    {
        sem_wait(semaforoMonitor);

        configuracion = leer_configuracion("../Archivos_datos/config.txt");

        sem_wait(semaforo_log);
        AgregarLog("Se esta leyendo el documento de transacciones en modo lectura");
        sem_post(semaforo_log);
        FILE *archivo = fopen(Transacciones, "r");
        char linea[256];

        // leo el fichero
        while (fgets(linea, sizeof(linea), archivo) != NULL)
        {
            int id;
            char fecha[50];
            char tipo[50];
            float cantidad;

            if (sscanf(linea, "%d;%49[^;];%49[^;];%f", &id, fecha, tipo, &cantidad) == 4)
            {
                if (strcmp(tipo, "retiro") == 0 && cantidad > configuracion.limite_retiro)
                {
                    sem_wait(semaforo_log);
                    AgregarLog("Se ha notificado al banco de la anomalia");
                    sem_post(semaforo_log);
                    printf("ALERTA retiro muy alto: Usuario %d en fecha %s realizó un retiro de %.2f\n", id, fecha, cantidad);
                }
            }
        }
        fclose(archivo);
        sem_post(semaforoMonitor);
    }
    return NULL;
}

void *TransferenciaSuperior(void *arg)
{
    while (1)
    {
        sem_wait(semaforoMonitor);

        configuracion = leer_configuracion("../Archivos_datos/config.txt");

        sem_wait(semaforo_log);
        AgregarLog("Se esta leyendo el documento de transacciones en modo lectura");
        sem_post(semaforo_log);
        FILE *archivo = fopen(Transacciones, "r");
        char linea[256];

        // leo el fichero
        while (fgets(linea, sizeof(linea), archivo) != NULL)
        {
            int id;
            char fecha[50];
            char tipo[50];
            float cantidad;

            if (sscanf(linea, "%d;%49[^;];%49[^;];%f", &id, fecha, tipo, &cantidad) == 4)
            {
                if (strcmp(tipo, "transferencia") == 0 && cantidad > configuracion.limite_transferencia)
                {
                    // paso el dato a banco
                    sem_wait(semaforo_log);
                    AgregarLog("Se ha notificado al banco de la anomalia");
                    sem_post(semaforo_log);
                    printf("ALERTA tranferecia muy alta: Usuario %d en fecha %s realizó una transferencia de %.2f\n", id, fecha, cantidad);
                }
            }
        }
        fclose(archivo);
        sem_post(semaforoMonitor);
    }
    return NULL;
}