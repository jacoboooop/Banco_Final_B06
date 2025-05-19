#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "comun.h"

void *realizar_deposito(void *u);
void *realizar_retiro(void *u);
void *realizar_transferencia(void *u);
void *consultar_saldo(void *u);
void *Estado_banco(void *arg);
void Manejar_Ctr();
void EscribirTransacciones(int ID, const char tipo[], float cantidad);

char mensaje[100];

sem_t *semaforo, *semaforo_log;
pthread_t thread_deposito, thread_retiro, thread_transferencia;
TablaCuentas *tabla;

int cerrar = 0;

int main(int argc, char *argv[])
{
    AgregarLog("ha llegado al usuario");
    semaforo = sem_open("semaforo_cuentas", 0);
    semaforo_log = sem_open("semaforo_log", 0);

    // sem_post(semaforo);

    int numero_cuenta = atoi(argv[1]); // guardo ncuenta para operaciones

    pthread_t banco;
    pid_t pid_banco = atoi(argv[2]);
    pthread_create(&banco, NULL, Estado_banco, &pid_banco);

    int shm_id = atoi(argv[3]);
    sem_wait(semaforo_log);
    AgregarLog("Mapeamos la memoria");
    sem_post(semaforo_log);
    tabla = (TablaCuentas *)shmat(shm_id, NULL, 0);

    Config configuracion = leer_configuracion("../Archivos_datos/config.txt");

    signal(SIGINT, Manejar_Ctr);

    int opcion;
    while (1)
    {
        system("clear");
        printf("1. Depósito\n2. Retiro\n3. Transferencia\n4. Consultar saldo\n5. Salir\n");
        printf("\n<----- ¿Que operación desea relaizar?: ");
        scanf("%d", &opcion);
        switch (opcion)
        {
        case 1:
            pthread_create(&thread_deposito, NULL, realizar_deposito, &numero_cuenta);
            pthread_join(thread_deposito, NULL);
            break;
        case 2:
            pthread_create(&thread_retiro, NULL, realizar_retiro, &numero_cuenta);
            pthread_join(thread_retiro, NULL);
            break;
        case 3:
            int resultado;
            resultado = pthread_create(&thread_transferencia, NULL, realizar_transferencia, &numero_cuenta);
            resultado = pthread_join(thread_transferencia, NULL);
            break;
        case 4:
            consultar_saldo(&numero_cuenta);
            break;
        case 5:
            sem_close(semaforo);
            sem_unlink("semaforo_cuentas");
            shmdt(tabla);
            exit(0);
        default:
            printf("Esa opcion no es posible, vuelva a intentarlo");
        }
        if (cerrar == 1)
        {
            char comando[200];
            snprintf(comando, sizeof(comando), "kill -9 %d", getpid());
            system(comando);
        }
    }

    return 0;
}

void *realizar_deposito(void *u)
{

    sem_wait(semaforo_log);
    AgregarLog("Se esta esperando a que el semaforo para hacer un deposito");
    sem_post(semaforo_log);
    sem_wait(semaforo);

    int NumeroUsuario;
    int NumeroMemoria;
    float deposito = 0;
    NumeroUsuario = *(int *)u;



    system("clear");

    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        if (tabla->cuentas[i].numero_cuenta == NumeroUsuario)
        {
            printf("%s tiene un saldo de %f\n", tabla->cuentas[i].titular, tabla->cuentas[i].saldo);
            printf("\t¿Cuento dinero quiere depositar?: ");
            scanf(" %f", &deposito);
            tabla->cuentas[i].saldo += deposito;
            system("clear");
            printf("El deposito se ha realizado con exito.");
            printf("\nSaldo actual: %f", tabla->cuentas[i].saldo);
            printf("\nVolviendo al menu...");
            if (deposito != 0)
            {
                EscribirTransacciones(tabla->cuentas[i].numero_cuenta, "deposito", deposito);
            }
        }
    }

    snprintf(mensaje, sizeof(mensaje), "Se ha realizado un deposito de: %f", deposito);
    sem_wait(semaforo_log);
    AgregarLog(mensaje);
    sem_post(semaforo_log);

    sem_wait(semaforo_log);
    AgregarLog("Se ha liberado el semaforo");
    sem_post(semaforo_log);
    sem_post(semaforo);
    sleep(4);
    system("clear");
}

void *realizar_retiro(void *u)
{
    sem_wait(semaforo_log);
    AgregarLog("Se esta esperando a que el semaforo para hacer un retiro");
    sem_post(semaforo_log);
    sem_wait(semaforo);

    int NumeroUsuario;
    NumeroUsuario = *(int *)u;

    float retiro;

    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        if (tabla->cuentas[i].numero_cuenta == NumeroUsuario)
        {
            system("clear");
            printf("%s tiene un saldo de %f\n", tabla->cuentas[i].titular, tabla->cuentas[i].saldo);
            printf("\t¿Cuento dinero quiere retirar?: ");
            scanf(" %f", &retiro);

            if (retiro > tabla->cuentas[i].saldo)
            {
                printf("No tiene suficiente dinero");

                sem_wait(semaforo_log);
                AgregarLog("Se ha liberado el semaforo");
                sem_post(semaforo_log);
                sem_post(semaforo);
                sem_wait(semaforo_log);
                AgregarLog("El usuario no tiene suficiente dinero");
                sem_post(semaforo_log);
            }
            else
            {
                retiro = retiro * (-1);
                tabla->cuentas[i].saldo += retiro;
                system("clear");
                printf("El retiro se ha realizado con exito.");
                printf("\nSaldo actual: %f", tabla->cuentas[i].saldo);
                printf("\nVolviendo al menu...");
                sleep(3);
                // actualizo el archivo

                if (retiro != 0)
                {
                    EscribirTransacciones(tabla->cuentas[i].numero_cuenta, "retiro", retiro);
                }

                sem_wait(semaforo_log);
                AgregarLog("Se ha liberado el semaforo");
                sem_post(semaforo_log);
                sem_post(semaforo);
                system("clear");

                snprintf(mensaje, sizeof(mensaje), "Se ha realizado un retiro de: %f", retiro);
                sem_wait(semaforo_log);
                AgregarLog(mensaje);
                sem_post(semaforo_log);
            }
        }
    }
}

void *realizar_transferencia(void *u)
{
    sem_wait(semaforo_log);
    AgregarLog("Se esta esperando a que el semaforo para hacer una transaccion");
    sem_post(semaforo_log);
    sem_wait(semaforo);

    int NumeroUsuario;
    int NumeroDestino;
    int MemoriaDestino;
    int MemoriaUsuario;
    int Verif = 0;
    float Cantidad = 0;

    NumeroUsuario = *(int *)u;

    sem_wait(semaforo_log);
    AgregarLog("Se esta abriendo el fichero para hacer una transaccion");
    sem_post(semaforo_log);

    printf("\nDima el numero de cuenta al que quieres hacer una transferencia: ");
    scanf("%d", &NumeroDestino);
    printf("\nQue cantidad le quieres ingresar: ");
    scanf("%f", &Cantidad);

    if ((NumeroDestino = 0) || (Cantidad = 0))
    {
        printf("\nError, tienes que llenar todos los campos\n");
        sleep(2);
        return 0;   
    }

    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        if (NumeroUsuario == tabla->cuentas[i].numero_cuenta)
        {
            if (Cantidad > tabla->cuentas[i].saldo)
            {
                sem_wait(semaforo_log);
                AgregarLog("Saldo para la transaccion insuficiente");
                sem_post(semaforo_log);
                printf("La operacion supera su saldo.");

                sem_wait(semaforo_log);
                AgregarLog("Se ha liberado el semaforo");
                sem_post(semaforo_log);
                sem_post(semaforo);
                return 0;
            }
            MemoriaUsuario = i;
        }
        else if (NumeroDestino == tabla->cuentas[i].numero_cuenta){
            Verif = 1;
            MemoriaDestino = i;
        }
    }


    if (Verif = 1){
        tabla->cuentas[MemoriaUsuario].saldo -= Cantidad;
        tabla->cuentas[MemoriaDestino].saldo += Cantidad;
        EscribirTransacciones(tabla->cuentas[MemoriaDestino].numero_cuenta, "transacciones", Cantidad);
        EscribirTransacciones(tabla->cuentas[MemoriaUsuario].numero_cuenta, "transacciones", Cantidad);
    }


    sem_wait(semaforo_log);
    AgregarLog("Se ha liberado el semaforo");
    sem_post(semaforo_log);
    sem_post(semaforo);

    snprintf(mensaje, sizeof(mensaje), "Se ha realizado una transferencia de: %f", Cantidad);
    sem_wait(semaforo_log);
    AgregarLog(mensaje);
    sem_post(semaforo_log);

    return 0; // Transferencia exitosa.
}

void *consultar_saldo(void *u)
{

    char confirmacion;
    int NumeroUsuario;
    NumeroUsuario = *(int *)u;

    system("clear");

    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        if (tabla->cuentas[i].numero_cuenta == NumeroUsuario)
        {
            printf("Tienes un saldo de %f\n", tabla->cuentas[i].saldo);
        }
    }
    printf("¿Desea volver al menú?: ");
    scanf(" %c", &confirmacion);
    do
    {
        if (confirmacion == 's' || confirmacion == 'S')
        {
            break;
        }
        else if (confirmacion == 'N' || confirmacion == 'n')
        {
        }
        else
        {
            printf("Opción no válida, vuelva a intentarlo.");
        }
    } while (confirmacion != 'S' && confirmacion != 's' && confirmacion != 'N' && confirmacion != 'n');

    system("clear");
}

void Manejar_Ctr()
{

    char confirmacion;
    int respuesta = 0;

    system("clear");
    printf("\n¿Seguro que quieres cerrar sesion ? (s/N): ");
    scanf(" %c", &confirmacion);
    do
    {
        if (confirmacion == 's' || confirmacion == 'S')
        {
            cerrar = 1;
            sem_close(semaforo);
            sem_unlink("semaforo_cuentas");
            shmdt(tabla);
            exit(0);
            break;
        }
        else if (confirmacion == 'N' || confirmacion == 'n')
        {
            break;
        }
        else
        {
            system("clear");
            printf("Opción no válida, vuelva a intentarlo.");
            sleep(3);
            break;
        }
    } while (respuesta == 0);

    return;
}

void EscribirTransacciones(int ID, const char tipo[], float cantidad)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    char ruta[100];
    snprintf(ruta, sizeof(ruta), "../Archivos_datos/transacciones/%d/Transacciones.log", ID);

    FILE *file = fopen(ruta, "a");
    fprintf(file, "%d;%d/%02d/%02d;%s;%f\n", ID, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tipo, cantidad);
    fclose(file);
}