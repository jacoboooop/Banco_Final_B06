#ifndef COMUN_H
#define COMUN_H

typedef struct Cuenta{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int bloqueado;
} Cuenta;

typedef struct TablaCuentas {
    Cuenta cuentas[100];
    int num_cuentas;
} TablaCuentas;

typedef struct BufferEstructurado {
    Cuenta operaciones[10];
    int inicio;
    int fin;
} BufferEstructurado;

typedef struct Config {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

#define FIFO_BANCO_MONITOR "/tmp/fifo_banco_monitor"

Config leer_configuracion(const char *ruta);

void AgregarLog(const char *operacion);

void* Estado_banco(void* arg);

#endif

