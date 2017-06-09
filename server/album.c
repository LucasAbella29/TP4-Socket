#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../utils/definitions.h"
#include "album.h"

BOOLEAN crear_album(char * nombre, char * usuario){
	char folder [] = "albumes/";
	char * route = (char *)malloc(strlen(folder)+ strlen("/") + strlen(nombre) + strlen(usuario));

	strcpy(route, folder);
	strcat(route, usuario);
	strcat(route, "/");
	strcat(route, nombre);

	if(mkdir(route, 0777) == 0){
		return TRUE;
	}
	return FALSE;
}

BOOLEAN registrar_album(char * nombre, char * usuario){
	char folder [] = "albumes/";
	char * route = (char *)malloc(strlen(folder)+ strlen(usuario) + strlen("/albumes.txt"));
	int id;
	FILE * archivo;

	strcpy(route, folder);
	strcat(route, usuario);
	strcat(route, "/albumes.txt");

	id = siguiente_id(route);

	archivo = fopen(route, "a");

	if(archivo != NULL){
		fprintf(archivo, "%s %d\n", nombre, id);
		fclose(archivo);

		return TRUE;
	}
	return FALSE;
}

int siguiente_id(char * path){
	FILE * archivo;
	char album[MAX_NOMBRE_SOLICITUD];
	int id = -1;
	int mayor = 0;

	archivo = fopen(path, "r");

	if(archivo != NULL){
		while(fscanf(archivo, "%s", album) > 0){
			fscanf(archivo, "%d", &id);
			if(id > mayor){
				mayor = id;
			}
		}
		fclose(archivo);
	}

	return mayor+1;
}

BOOLEAN crear_carpeta_general_albumes(void){
	char folder [] = "albumes";

	if(mkdir(folder, 0777) == 0){
		return TRUE;
	}
	return FALSE;
}

char * buscar_album_id(char * usuario, int id_album){
	FILE * archivo;
	char album[MAX_NOMBRE_SOLICITUD];
	int id = -1;
	char folder [] = "albumes/";
	char * route = (char *)malloc(strlen(folder)+ strlen(usuario) + strlen("/albumes.txt"));
	char * respuesta = NULL;

	strcpy(route, folder);
	strcat(route, usuario);
	strcat(route, "/albumes.txt");

	archivo = fopen(route, "r");

	if(archivo != NULL){
		while(fscanf(archivo, "%s", album) > 0){
			fscanf(archivo, "%d", &id);
			if(id == id_album){
				respuesta = (char *)malloc(sizeof(char) * strlen(album));
				strcpy(respuesta,album);
				break;
			}
		}
		fclose(archivo);
	}

	return respuesta;
}

char * crear_ruta(char * usuario, char * album, char * archivo){
	char folder [] = "albumes/";
	int cantidad_barras = 2;
	char * ruta = (char *)malloc(strlen(folder) + strlen(usuario) + strlen(album) + strlen(archivo) + cantidad_barras);

	strcpy(ruta, folder);
	strcat(ruta, usuario);
	strcat(ruta, "/");
	strcat(ruta, album);
	strcat(ruta, "/");
	strcat(ruta, archivo);

	return ruta;
}

BOOLEAN eliminar_album(char * nombre, char * usuario,int id){
	char folder [] = "rm -R albumes/";
	char * route = (char *)malloc(strlen(folder)+ strlen("/") + strlen(nombre) + strlen(usuario));

	strcpy(route, folder);
	strcat(route, usuario);
	strcat(route, "/");
	strcat(route, nombre);

	if(system(route) == 0){
		eliminar_album_de_lista(usuario,id);
		return TRUE;
	}
	return FALSE;
}

BOOLEAN eliminar_album_de_lista( char * usuario, int id_album ){
	FILE * archivo;
	FILE * aux;
	BOOLEAN resultado = FALSE;
	char album[MAX_NOMBRE_SOLICITUD];
	int id = -1;

	char folder [] = "albumes/";
	char * route = (char *)malloc(strlen(folder)+ strlen("/") + strlen(usuario)+ strlen(ARCHIVO_ALBUM ));

	strcpy(route, folder);
	strcat(route, usuario);
	strcat(route, "/");
	strcat(route, ARCHIVO_ALBUM);
	printf("route : %s\n",route );
	archivo = fopen(route, "r");
	aux = fopen(ARCHIVO_AUXILIAR_ALBUM, "w");
	if(archivo != NULL && aux != NULL){
		while(fscanf(archivo, "%s", album) > 0){
			fscanf(archivo, "%d", &id);
			printf("------------------- %s : %d\n",album,id );
			if(id != id_album){
				printf("%s : %d\n",album,id );
				fprintf(aux, "%s %d\n", album, id);
			}
		}
		fclose(archivo);
		fclose(aux);

		remove(route);
		rename(ARCHIVO_AUXILIAR_ALBUM, route);
		remove(ARCHIVO_AUXILIAR_ALBUM);
		resultado = TRUE;
	}
	return resultado;
}
