
#include "../utils/definitions.h"

#define ARCHIVO_USUARIOS "usuarios.txt"

typedef struct usuario{
	char usuario [MAX_USUARIO];
	char clave [MAX_CLAVE];
	char nombre [MAX_NOMBRE];
	char apellido [MAX_APELLIDO];
}USUARIO;

USUARIO * buscar_usuario(char *);
BOOLEAN agregar_usuario(char *, char *, char *, char *);
BOOLEAN crear_carpeta_usuario(char * );
BOOLEAN listar_usuarios(FILE *);