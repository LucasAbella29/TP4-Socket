#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../utils/definitions.h"
#include "../utils/mensajes_utils.h"
#include "menu.h"
#include "client_ftp.h"
#include "client.h"

int id_usuario = -1;
char * direccion;
int puerto_servidor;

int main(int argc, char ** argv) {
	int descriptor;
	struct sockaddr_in dir;

	/*---------------------------------------------------------------------*
	 * Verificar los argumentos
	 *---------------------------------------------------------------------*/
	if (argc < 4) {
		printf("Uso: cliente <direccion>\n");
		printf("     Donde: <direccion> = <ip> <puerto_udp> <puerto_servidor_tcp>\n");
		exit(-1);
	}

	direccion = argv[1];
	puerto_servidor = atoi(argv[3]);

	/*--------------------------------------------------------------------*
	 * Establecer la dirección del servidor y conectarse
	 *--------------------------------------------------------------------*/
	bzero((char *) &dir, sizeof(dir)); // Se blanquea toda la estructura
	dir.sin_family = AF_INET; // (Address Family)
	if (inet_pton( AF_INET, argv[1], &dir.sin_addr) <= 0) { // Se trasforma la ip de texto a tipo ip (binario).
		perror("Cliente UDP: Error en la función inet_pton.");
		exit(-1);
	}
	dir.sin_port = htons(atoi(argv[2])); // Se convierte el numero de puerto a un dato de la red.

	if ((descriptor = socket( AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Cliente UDP: No se pudo crear el socket.\n");
		exit(-1);
	}

	/*---------------------------------------------------------------------*
	 * Realizar la función del cliente
	 *---------------------------------------------------------------------*/
	principal( stdin, descriptor, (struct sockaddr *) &dir, sizeof(dir));
	return (0);
}

/*-------------------------------------------------------------------------*
 * principal()
 *-------------------------------------------------------------------------*/
int principal(FILE *fp, int sockfd, const struct sockaddr *dir, socklen_t sa) {
	int n;
	char linea_env[MAXLINEA], linea_rcb[MAXLINEA + 1];

	// Inicio de sesion y registro.
	MENU_ITEM menu_principal [ITEMS_MENU_PRINCIPAL] = {
														{"Iniciar Sesion", iniciar_sesion},
														{"Registrarse", registro},
														{"Salir", salir}
													};
	MENU_ITEM menu_sesion [ITEMS_MENU_SESION] = {
												{"Listar Album", listar_albumes},
												{"Crear Album", crear_album},
												{"Modificar Album", modificar_album},
												{"Eliminar Album", eliminar_album},
												{"Listar Archivos Album", listar_archivos},
												{"Subir Archivo Album", subir_archivo_album},
												{"Descargar Archivo Album", descargar_archivo_album},
												{"Modificar Archivo Album", modificar_archivo},
												{"Eliminar Archivo Album", eliminar_archivo},
												{"Compartir Album Usuario", compartir_album_usuario},
												{"Dejar Compartir Album Usuario", dejar_compartir_album_usuario},
												{"Listar Usuarios", listar_usuarios},
												{"Cerrar Sesion", cerrar_sesion}
											};

	int items_menu_actual;
	ITEM_MENU menu_actual;
	int opcion;
	void * mensaje;
	int longitud_mensaje = 0;

	for(;;){
		if(id_usuario != -1){
			menu_actual = menu_sesion;
			items_menu_actual = ITEMS_MENU_SESION;
		}
		else{
			menu_actual = menu_principal;
			items_menu_actual = ITEMS_MENU_PRINCIPAL;
		}

		imprimir_menu(menu_actual, items_menu_actual);
		opcion = obtener_opcion();
		mensaje = realizar_operacion(menu_actual, items_menu_actual, opcion, &longitud_mensaje);
		if(longitud_mensaje > 0){
			sendto(sockfd, (void *) mensaje, longitud_mensaje, 0,dir, sa);

			n = recvfrom( sockfd, linea_rcb, MAXLINEA, 0, NULL, NULL );
			analizar_respuesta(linea_rcb);
		}
	}

	return 0;

}

void * salir(int * longitud){
	printf("Adios!\n");
	exit(0);
	return NULL;
}

void * iniciar_sesion(int * longitud){
	char usuario[MAX_USUARIO];
	char clave[MAX_CLAVE];

	// TODO esta lineas sirven? para que?
	// bzero((char *) usuario, sizeof(char) * MAX_USUARIO);
	// bzero((char *) clave, sizeof(char) * MAX_CLAVE);

	obtener_datos_iniciar_sesion(usuario,clave);
	return mensaje_iniciar_sesion( usuario , clave ,longitud );
}

void obtener_datos_iniciar_sesion( char * usuario , char * clave ){
	obtener_datos("Ingrese su nombre de usuario: ","%s",usuario);
	obtener_datos("Ingrese su contraseña: ","%s",clave);
}

void obtener_datos( char * ouput , char * tipo , void * dato ){
		printf(ouput);
		scanf(tipo, dato);
}

void * registro(int * longitud){
	char nombre[MAX_NOMBRE];
	char apellido[MAX_APELLIDO];
	char usuario[MAX_USUARIO];
	char clave[MAX_CLAVE];

	obtener_datos_registro(nombre,apellido,usuario,clave);
	return mensaje_registro(nombre,apellido,usuario,clave,longitud);
}

void obtener_datos_registro(char * nombre , char * apellido , char * usuario , char * clave ){

	obtener_datos("Ingrese su nombre: ","%s",nombre);
	obtener_datos("Ingrese su nombre de apellido: ","%s",apellido);
	obtener_datos("Ingrese su nombre de usuario: ","%s",usuario);
	obtener_datos("Ingrese su contraseña: ","%s",clave);
}

void analizar_respuesta(char * linea_rcb){
	ERROR * mensaje_error;
	CONFIRMAR * mensaje_confirmacion;

	switch(*linea_rcb){
	case M_ERROR:
		mensaje_error = (ERROR *)linea_rcb;
		printf("Cliente UDP: Error: %s\n", mensaje_error->mensaje);
		break;
	case M_CONFIRMAR:
		mensaje_confirmacion = (CONFIRMAR *)linea_rcb;
		if(mensaje_confirmacion->ID_Usuario > 0){
			id_usuario = mensaje_confirmacion->ID_Usuario;
		}
		else{
			if(mensaje_confirmacion->ID_SUB_OP == 0){
				id_usuario = -1;
			}
		}
		printf("Cliente UDP: %s\n", mensaje_confirmacion->mensaje);
		break;
	default:
		printf("Cliente UDP: Error: La respuesta brindada por el servidor no pudo ser reconocida.\n");
		break;
	}

}

void * cerrar_sesion(int * longitud){
	return mensaje_cerrar_sesion(id_usuario, longitud);
}

void * crear_album(int * longitud){
	char nombre_album [MAX_NOMBRE_SOLICITUD];

	obtener_datos("Ingrese el nombre del nuevo album: ","%s",nombre_album);
	return mensaje_solicitud( id_usuario, SubOP_Crear_album , 0 , 0 , nombre_album, longitud );

}

void * eliminar_album(int * longitud){
	int id_album;
	obtener_datos("Ingrese el id del album a eliminar: ","%d",&id_album);
	char nombre_album [MAX_NOMBRE_SOLICITUD];
	// obtener_datos("Ingrese el nombre del album a eliminar\n","%s",nombre_album);
	// return mensaje_solicitud( id_usuario+ '0' , SubOP_Eliminar_album , '0' , '0' , nombre_album, longitud );
	return mensaje_solicitud( id_usuario, SubOP_Eliminar_album , id_album, 0, nombre_album, longitud );

}


void * subir_archivo_album(int * longitud){
	int sockid;
	BOOLEAN resultado;
	char ruta[MAXPATH];
	char usuario[MAX_USUARIO];
	int id_album;

	printf("Ingrese la ruta del archivo que desea subir: ");
	scanf("%s",ruta);
	printf("Ingrese el nombre del usuario propietario del album: ");
	scanf("%s",usuario);
	printf("Ingrese el id del album al que desea subirlo: ");
	scanf("%d", &id_album);

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	*longitud = 0;

	resultado = subir_archivo(ruta, sockid, usuario, id_usuario, id_album);

	close(sockid);

	return NULL;
}

void * descargar_archivo_album(int * longitud){
	int sockid;
	BOOLEAN resultado;
	char ruta[MAXPATH];
	char usuario[MAX_USUARIO];
	int id_album;
	int id_archivo;

	printf("Ingrese la ruta donde desea almacenar el archivo: ");
	scanf("%s",ruta);
	printf("Ingrese el nombre del usuario propietario del album: ");
	scanf("%s",usuario);
	printf("Ingrese el id del album donde se encuentra el archivo: ");
	scanf("%d", &id_album);
	printf("Ingrese el id del archivo: ");
	scanf("%d", &id_archivo);

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	*longitud = 0;

	resultado = descargar_archivo(ruta, sockid, usuario, id_usuario, id_album, id_archivo);

	close(sockid);

	return NULL;
}

void * listar_albumes(int * longitud){
	int sockid;
	BOOLEAN resultado;

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	*longitud = 0;

	resultado = listar_albumes_usuario(sockid, id_usuario);

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	listar_albumes_compartidos_usuario(sockid, id_usuario);

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	listar_albumes_compartidos_otros(sockid, id_usuario);

	close(sockid);

	return NULL;
}

void * listar_archivos(int * longitud){
	int sockid;
	BOOLEAN resultado;
	int id_album;

	printf("Ingrese el id del album: ");
	scanf("%d", &id_album);

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	*longitud = 0;

	resultado = listar_archivos_usuario(sockid, id_usuario, id_album);

	close(sockid);

	return NULL;
}

void * modificar_album(int * longitud){
	SOLICITUD * mensaje_solicitud;
	int id_album;
	char nombre[MAX_NOMBRE_SOLICITUD];

	printf("Ingrese el id del album a modificar: ");
	scanf("%d", &id_album);
	printf("Ingrese el nuevo nombre para el album: ");
	scanf("%s", nombre);

	mensaje_solicitud = (SOLICITUD *)malloc(sizeof(SOLICITUD));

	mensaje_solicitud->OP = M_SOLICITUD;
	mensaje_solicitud->ID_Usuario = id_usuario;
	mensaje_solicitud->ID_SUB_OP = SubOP_Modificar_album;
	mensaje_solicitud->ID_Album = id_album;
	mensaje_solicitud->ID_Archivo = 0;
	strcpy(mensaje_solicitud->nombre, nombre);

	*longitud = sizeof(SOLICITUD);

	return (void *)mensaje_solicitud;
}

void * eliminar_archivo(int * longitud){
	int id_album;
	int id_archivo;

	obtener_datos("Ingrese el id del album donde se encuentra el archivo: ","%d",&id_album);
	obtener_datos("Ingrese el id del archivo: ","%d",&id_archivo);

	return mensaje_solicitud(id_usuario, SubOP_Eliminar_archivo_album , id_album, id_archivo, "", longitud);
}

void * modificar_archivo(int * longitud){
	SOLICITUD * mensaje_solicitud;
	int id_album;
	int id_archivo;
	char nombre[MAX_NOMBRE_SOLICITUD];

	printf("Ingrese el id del album donde se encuentra el archivo: ");
	scanf("%d", &id_album);
	printf("Ingrese el id del archivo a modificar: ");
	scanf("%d", &id_archivo);
	printf("Ingrese el nuevo nombre para el archivo (sin extension): ");
	scanf("%s", nombre);

	mensaje_solicitud = (SOLICITUD *)malloc(sizeof(SOLICITUD));

	mensaje_solicitud->OP = M_SOLICITUD;
	mensaje_solicitud->ID_Usuario = id_usuario;
	mensaje_solicitud->ID_SUB_OP = SubOP_Modificar_archivo_album;
	mensaje_solicitud->ID_Album = id_album;
	mensaje_solicitud->ID_Archivo = id_archivo;
	strcpy(mensaje_solicitud->nombre, nombre);

	*longitud = sizeof(SOLICITUD);

	return (void *)mensaje_solicitud;
}

void * listar_usuarios(int * longitud){
	int sockid;
	BOOLEAN resultado;

	sockid = iniciar_cliente_ftp(puerto_servidor, direccion);

	*longitud = 0;

	resultado = listar_usuarios_registrados(sockid, id_usuario);

	close(sockid);

	return NULL;
}

void * compartir_album_usuario(int * longitud){
	SOLICITUD * mensaje_solicitud;
	int id_album;
	char nombre[MAX_USUARIO];

	printf("Ingrese el id del album que desea compartir: ");
	scanf("%d", &id_album);
	printf("Ingrese el usuario al que le gustaria compartirle el album: ");
	scanf("%s", nombre);

	mensaje_solicitud = (SOLICITUD *)malloc(sizeof(SOLICITUD));

	mensaje_solicitud->OP = M_SOLICITUD;
	mensaje_solicitud->ID_Usuario = id_usuario;
	mensaje_solicitud->ID_SUB_OP = SubOP_Compartir_album_usuario;
	mensaje_solicitud->ID_Album = id_album;
	mensaje_solicitud->ID_Archivo = 0;
	strcpy(mensaje_solicitud->nombre, nombre);

	*longitud = sizeof(SOLICITUD);

	return (void *)mensaje_solicitud;
}

void * dejar_compartir_album_usuario(int * longitud){
	SOLICITUD * mensaje_solicitud;
	int id_album;
	char nombre[MAX_USUARIO];

	printf("Ingrese el id del album que desea dejar de compartir: ");
	scanf("%d", &id_album);
	printf("Ingrese el usuario al que le gustaria dejar de compartirle el album: ");
	scanf("%s", nombre);

	mensaje_solicitud = (SOLICITUD *)malloc(sizeof(SOLICITUD));

	mensaje_solicitud->OP = M_SOLICITUD;
	mensaje_solicitud->ID_Usuario = id_usuario;
	mensaje_solicitud->ID_SUB_OP = SubOP_Dejar_compartir_album_usuario;
	mensaje_solicitud->ID_Album = id_album;
	mensaje_solicitud->ID_Archivo = 0;
	strcpy(mensaje_solicitud->nombre, nombre);

	*longitud = sizeof(SOLICITUD);

	return (void *)mensaje_solicitud;
}