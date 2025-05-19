#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct Cuenta {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
};

void CrearArchivo(int numeroCuenta);

int main(){

    char nombre_archivo[] = "../Archivos_datos/cuentas.dat";

    FILE *file = fopen(nombre_archivo, "w");

    struct Cuenta cuentas;

    for (int i = 1; i < 6; i++){
        cuentas.numero_cuenta = i+1000;
        sprintf(cuentas.titular, "usuario%d", i);
        fprintf(file, "%d,%s,500,0\n", cuentas.numero_cuenta, cuentas.titular);
        CrearArchivo(i+1000);
    }

    fclose(file);
    printf("Archivo de cuentas inicializado correctamente.\n");   

    return 0; 

}


void CrearArchivo (int numeroCuenta) {

    char ruta[100];
    snprintf(ruta, sizeof(ruta), "../Archivos_datos/transacciones/%d", numeroCuenta);
    mkdir(ruta, 0777);

    // Crear el archivo dentro del directorio
    char ruta_archivo[256];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/Transacciones.log",ruta);

    FILE *archivo = fopen(ruta_archivo, "w");

    if (archivo == NULL) {
        perror("Error al intentar crear el archivo");
        return;
    }

    fclose(archivo);

    return;
}