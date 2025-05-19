#include <stdio.h>
#include <unistd.h>
#include <signal.h>
// Manejador para SIGINT (Ctrl + C)
void manejador(int signo) {
printf("\nSeñal %d recibida. Terminando programa.\n",
signo);
}
int main() {
signal(SIGINT, manejador); // Capturar Ctrl + C
printf("Esperando señales... (Presiona Ctrl + C parasalir)\n"); // Se suspende hasta que reciba una señal
while (1) {
    printf("Fin del programa.\n");
    sleep(1);
}
return 0;
}