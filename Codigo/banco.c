#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ipc.h>
#include "comun.h"

#define MAX_LINE_LENGTH 256

void menu();
int RegistrarCuenta(const char *nombre_input);
void LoginCuenta(int numeroCuenta);
void init_cuentas(const char *nombre_archivo);
bool VerificarCuenta(char nombre[], int numero, const char *nombreArchivo);
void Manejar_Ctrl();
void *escucharPipe(void *arg);
void CrearArchivo(int numeroCuenta);
void LlenarMemoria();
void *GestionBuffer();
void GuardarEnArchivo();

Config configuracion;

Cuenta usuario;
sem_t *semaforo, *semaforo_log;
int shm_id;
TablaCuentas *tabla;
pthread_t thread_Buffer;

int main()
{

    
    semaforo = sem_open("semaforo_cuentas", O_CREAT, 0644, 1);
    semaforo_log = sem_open("semaforo_log", O_CREAT, 0644, 1);
    semaforo_log = sem_open("semaforo_log", 0);

    sem_wait(semaforo_log);
    AgregarLog("EL banco se ha iniciado");
    sem_post(semaforo_log);

    sem_wait(semaforo_log);
    AgregarLog("Se crean los semaforos de log y cuentas");
    sem_post(semaforo_log);

    system("clear");

    int valor = sem_getvalue(semaforo, &valor);
    if (valor == 0)
    {
        sem_wait(semaforo_log);
        AgregarLog("El semaforo no se cerro correctamente");
        sem_post(semaforo_log);
        sem_post(semaforo);
    }

    signal(SIGINT, Manejar_Ctrl);

    configuracion = leer_configuracion("../Archivos_datos/config.txt");

    //*******************************inicializo el monitor***********************************

    sem_wait(semaforo_log);
    AgregarLog("Creo el pipe y el proceso monitor");
    sem_post(semaforo_log);

    pthread_t hiloPipe; // creo un hilo para el pipe
    int fd[2];
    pipe(fd);
    pid_t pid = fork();

    char mensaje[250];
    pid_t pid_banco = getpid();

    if (pid < 0)
    {
        sem_wait(semaforo_log);
        AgregarLog("Error al iniciar el monitor");
        sem_post(semaforo_log);
        perror("Error al crear el monitor");
        exit(1);
    }
    else if (pid == 0)
    {

        sem_wait(semaforo_log);
        AgregarLog("Proceso monitor creado correctamente");
        sem_post(semaforo_log);
        // hijo
        close(fd[0]); // Cierra lectura
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]); // Ya está duplicado, cerramos
        snprintf(mensaje, sizeof(mensaje), "%d", pid_banco);
        execl("./monitor", "monitor", mensaje, NULL);
        perror("Error en exec de monitor");
        exit(1);
    }
    else
    {

        close(fd[1]); // solo va a leer
        pthread_create(&hiloPipe, NULL, escucharPipe, &fd);
        sem_wait(semaforo_log);
        AgregarLog("Se ha iniciado el menu");
        sem_post(semaforo_log);

        AgregarLog("Creamos la memoria compartida");
        // Creamos memoria compartida
        shm_id = shmget(IPC_PRIVATE, sizeof(TablaCuentas), IPC_CREAT | 0666);
        tabla = (TablaCuentas *)shmat(shm_id, NULL, 0);
        LlenarMemoria();

        pid_t buffer = fork();

        if (buffer < 0)
        {
            sem_wait(semaforo_log);
            AgregarLog("Error al iniciar el Buffer");
            sem_post(semaforo_log);
            perror("Error al crear el buffer");
            exit(1);
        }
        else if (buffer== 0)
        {
            pthread_create(&thread_Buffer, NULL, GestionBuffer, NULL);
            pthread_join(thread_Buffer, NULL);
        }
        else {
            menu(configuracion.archivo_cuentas);
            close(fd[0]);
            sem_close(semaforo);
            sem_unlink("semaforo_cuentas");
        }

    }

    return (0);
}

void menu(const char *nombreArchivo)
{
    system("clear");
    int caso;
    printf("1. Login\n2. Registeer\n3. Salir\n");
    printf("\n<---¿Que desea hacer?: ");
    scanf("%d", &caso);
    char NombreUsuario[50];
    int NumeroCuenta;

    switch (caso)
    {
    case 1:
        sem_wait(semaforo_log);
        AgregarLog("Se esta intentando hacer login");
        sem_post(semaforo_log);
        // Proceso de login
        printf("_______________________________________________\n");
        printf("\nNombre de usuario: ");
        scanf("%s", NombreUsuario);
        printf("\nNumeroCuenta: ");
        scanf("%d", &NumeroCuenta);
        if (VerificarCuenta(NombreUsuario, NumeroCuenta, nombreArchivo))
        {
            LoginCuenta(NumeroCuenta);
            AgregarLog("Se ha hecho login");
        }
        else
        {
            system("clear");
            sem_wait(semaforo_log);
            AgregarLog("El usuario no existe");
            sem_post(semaforo_log);
            printf("\nEl usuario no existe\n");
            sleep(3);
        }
        menu(nombreArchivo);
        break;
    case 2:
        sem_wait(semaforo_log);
        AgregarLog("Se esta registrando un nuevo usuario");
        sem_post(semaforo_log);
        // registro
        printf("\nNombre de usuario: ");
        scanf("%s", NombreUsuario);
        int NumeroCuenta = RegistrarCuenta(NombreUsuario);
        sem_wait(semaforo_log);
        AgregarLog("Se ha registrado un nuevo usuario");
        sem_post(semaforo_log);
        printf("\nCuenta registrada correctamente...");
        printf("\n\nEl numero de cuenta asignando es el: %d", NumeroCuenta);
        sleep(3);
        menu(nombreArchivo);
        break;
    case 3:
        Manejar_Ctrl();
        return;
    default:
        sem_wait(semaforo_log);
        AgregarLog("Se ha introducido un parametor incorrecto");
        sem_post(semaforo_log);
        printf("\nOpción no válida. Inténtalo de nuevo.\n");
        break;
    }
}

void LoginCuenta(int numeroCuenta)
{

    pid_t pid_banco = getpid();
    pid_t pid2 = fork();

    if (pid2 < 0)
    {
        perror("Error al crear el usuario");
        exit(1);
    }
    else if (pid2 == 0)
    {
        sem_wait(semaforo_log);
        AgregarLog("Se crea el proceso para hacer login");
        sem_post(semaforo_log);
        sleep(1);
        char comando[250];
        snprintf(comando, sizeof(comando), "./usuario %d %d %d", numeroCuenta, pid_banco, shm_id);
        execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", comando, NULL);
        perror("Error en exec");
        exit(1);
    }
    else
    {
        printf("Esperando al inicio de sesion...\n");
        wait(NULL);
        printf("\nAccediendo al usuairo...\n\n");
    }
}

int RegistrarCuenta(const char *nombre_input)
{
    sem_wait(semaforo);

    sem_post(semaforo);

    tabla->cuentas[tabla->num_cuentas].numero_cuenta = tabla->num_cuentas + 1001;
    strcpy(tabla->cuentas[tabla->num_cuentas].titular, nombre_input);
    tabla->cuentas[tabla->num_cuentas].saldo = 500;
    tabla->cuentas[tabla->num_cuentas].bloqueado = 0;
    tabla->num_cuentas++;

    sem_wait(semaforo_log);
    AgregarLog("Se ha registrado un nuevo usuario en Memoria compartida");
    sem_post(semaforo_log);

    CrearArchivo(tabla->num_cuentas + 1000);
    return (tabla->num_cuentas + 1000);
}

bool VerificarCuenta(char nombre[], int numero, const char *nombreArchivo)
{
    sem_wait(semaforo_log);
    AgregarLog("Se busca en la memoria si esta el usuario");
    sem_post(semaforo_log);

    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        if (tabla->cuentas[i].numero_cuenta == numero && strcmp(tabla->cuentas[i].titular, nombre))
        {
            return true;
        }
    }

    sem_wait(semaforo_log);
    AgregarLog("No se ha encontrado al usuario en memoria, se procede a buscar en disco");
    sem_post(semaforo_log);

    FILE *file = fopen(nombreArchivo, "r");
    char linea[MAX_LINE_LENGTH];
    Cuenta usuario;
    while (fgets(linea, sizeof(linea), file) != NULL)
    {
        // Leemos todos los datos de la linea
        if (sscanf(linea, "%d,%49[^,],%f,%d", &usuario.numero_cuenta, usuario.titular, &usuario.saldo, &usuario.bloqueado) == 4)
        {
            // Verificar si el nombre y la contraseña coinciden
            if (strcmp(usuario.titular, nombre) == 0 && (usuario.numero_cuenta == numero))
            {
                tabla->cuentas[tabla->num_cuentas] = usuario;
                fclose(file);
                return true;
            }
        }
    }
    fclose(file);
    return false;
}

void Manejar_Ctrl()
{

    sem_wait(semaforo_log);
    AgregarLog("El usuario a pulsado CTRL+C");
    sem_post(semaforo_log);

    semaforo = sem_open("semaforo_cuentas", 0);
    int valor;
    bool respuesta = true;
    char confirmacion = 0;

    sem_getvalue(semaforo, &valor);

    system("clear");
    printf("\n¿Seguro que desea apagar el sisema ? (s/N): ");
    scanf(" %c", &confirmacion);
    do
    {
        if (confirmacion == 's' || confirmacion == 'S')
        {
            while (1)
            {
                if (valor == 0)
                {
                    system("clear");
                    printf("Hay usuarios haciendo operaciones, espere....");
                    sem_wait(semaforo_log);
                    AgregarLog("El usuario esta haciendo alguna operacion.");
                    sem_post(semaforo_log);
                    char comando[200];
                    GuardarEnArchivo();
                    sem_wait(semaforo_log);
                    AgregarLog("El programa se ha APAGADO correctamente y se han guardado los datos en el fichero");
                    sem_post(semaforo_log);
                    shmdt(tabla);
                    shmctl(shm_id, IPC_RMID, NULL);
                    snprintf(comando, sizeof(comando), "pkill -9 %d", getpid());
                    system(comando);
                }
                else
                {
                    sem_wait(semaforo_log);
                    AgregarLog("El programa se ha APAGADO correctamente.");
                    sem_post(semaforo_log);
                    shmdt(tabla);
                    shmctl(shm_id, IPC_RMID, NULL);
                    char comando[200];
                    snprintf(comando, sizeof(comando), "pkill -9 banco");
                    system(comando);
                }
            }
        }
        else if (confirmacion == 'N' || confirmacion == 'n')
        {
            menu(configuracion.archivo_cuentas);
        }
        else
        {
            system("clear");
            printf("Opción no válida, vuelva a intentarlo.");
            sleep(3);
            respuesta = false;
            break;
        }
    } while (respuesta == false);
}

void *escucharPipe(void *arg)
{
    int fd = *(int *)arg;
    char buffer[256];
    while (1)
    {
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0)
        {
            buffer[bytes] = '\0';
            printf("BANCO RECIBIÓ: %s\n", buffer);
        }
        else
        {
            usleep(2);
        }
    }
    return NULL;
}

void CrearArchivo(int numeroCuenta)
{

    char ruta[100];
    snprintf(ruta, sizeof(ruta), "../Archivos_datos/transacciones/%d", numeroCuenta);
    mkdir(ruta, 0777);

    // Crear el archivo dentro del directorio
    char ruta_archivo[256];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/Transacciones.log", ruta);

    FILE *archivo = fopen(ruta_archivo, "w");

    if (archivo == NULL)
    {
        perror("Error al intentar crear el archivo");
        return;
    }

    fclose(archivo);

    return;
}

void LlenarMemoria()
{

    Cuenta Aux;
    char linea[200];
    int i = 0;
    tabla->num_cuentas = 0;

    sem_wait(semaforo_log);
    AgregarLog("Abrimos el archivo de cuentas en modo lectura");
    sem_post(semaforo_log);
    FILE *file = fopen(configuracion.archivo_cuentas, "r");

    while (fgets(linea, sizeof(linea), file))
    {
        if (sscanf(linea, "%d,%49[^,],%f,%d", &Aux.numero_cuenta, Aux.titular, &Aux.saldo, &Aux.bloqueado) == 4)
        {
            sem_wait(semaforo_log);
            AgregarLog("Se ha agregado un usuario a la memoria");
            sem_post(semaforo_log);
            tabla->cuentas[i] = Aux;
            tabla->num_cuentas++;
            i++;
        }
    }

    return;
}

void *GestionBuffer()
{
    while (1)
    {
        GuardarEnArchivo();
        sleep(4);
    }
}

void GuardarEnArchivo()
{
    sem_wait(semaforo);
    FILE *file = fopen(configuracion.archivo_cuentas, "w");
    for (int i = 0; i < tabla->num_cuentas; i++)
    {
        fprintf(file, "%d,%s,%f,%d\n",
                tabla->cuentas[i].numero_cuenta,
                tabla->cuentas[i].titular,
                tabla->cuentas[i].saldo,
                tabla->cuentas[i].bloqueado);
    }
    fclose(file);
    sem_post(semaforo);
}