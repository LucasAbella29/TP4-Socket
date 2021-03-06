#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sesion.h"
#include "usuario.h"
#include "../utils/mensajes_utils.h"
#include "../utils/definitions.h"
#include "album.h"
#include "archivo.h"
#include "compartir.h"
#include "server_ftp.h"
#include "server.h"

void main(int argc, char *argv[]) {
	int descriptor;
	struct sockaddr_in dir_srv, dir_cli;
	int puerto_transferencia;
	int pid;

	/*---------------------------------------------------------------------*
	 * Verificar los argumentos recibidos								   *
	 *---------------------------------------------------------------------*/
	if (argc < 3) {
		printf("Uso: servidor <puerto> <puerto_transferencia>\n");
		exit(-1);
	}

	puerto_transferencia = atoi(argv[2]);

	/*--------------------------------------------------------------------*
	 * Inicializar el servidor                               			  *
	 *--------------------------------------------------------------------*/
	if ((descriptor = socket( AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Servidor UDP: Error creando el socket: %d\n", errno);
		exit(-1);
	}

	bzero((char *) &dir_srv, sizeof(dir_srv));
	dir_srv.sin_family = AF_INET;
	dir_srv.sin_addr.s_addr = htonl( INADDR_ANY);
	dir_srv.sin_port = htons(atoi(argv[1]));

	if (bind(descriptor, (struct sockaddr *) &dir_srv, sizeof(dir_srv)) < 0) {
		printf("Servidor UDP: Error al crear el enlace: %d\n", errno);
		exit(-1);
	}
	printf("Servidor UDP: Servidor iniciado.\n");

	pid = fork();

	if(pid == 0)
		iniciar_servidor_ftp(puerto_transferencia);
	else
		procesar(descriptor, (struct sockaddr *) &dir_cli, sizeof(dir_cli));

}

int id_user = 0;

/*-----------------------------------------------------------------------*
 * procesar() - realizar la tarea específica del servidor
 *-----------------------------------------------------------------------*/
void procesar(int descriptor, struct sockaddr *dir_cli_p, socklen_t longcli) {
	int recibido;
	int longitud_respuesta;
	socklen_t longitud;
	char msg[MAXLINEA];
	void* respuesta;

	crear_carpeta_general_albumes();

	for (;;) {
		longitud = longcli;
		// Recibe una solicitud del cliente
		recibido = recvfrom(descriptor, msg, MAXLINEA, 0, dir_cli_p, &longitud);
		// Llamado a la funcion
		respuesta = resolver(recibido, msg, &longitud_respuesta);
		// Retorna la respuesta al cliente.
		sendto(descriptor, (void *) respuesta, longitud_respuesta, 0, dir_cli_p, longitud);
	}
}

/*--------------------------------------------------------------------*
 * Se ocupa de resolver las peticiones del cliente                    *
 *--------------------------------------------------------------------*/
void* resolver(int longitud, char * mensaje, int * longitud_respuesta) {
	switch (*mensaje) {
	case M_INICIAR_SESION:
		return iniciar_sesion(mensaje, longitud_respuesta);
		break;
	case M_REGISTRO:
		return registrar(mensaje, longitud_respuesta);
		break;
	case M_SOLICITUD:
		return solicitud(mensaje, longitud_respuesta);
		break;
	case M_CERRAR_SESION:
		return cerrar_sesion(mensaje, longitud_respuesta);
		break;
	}
	printf("Servidor UDP: Error: El tipo de mensaje recibido no ha sido reconocido.\n");
	return mensaje_error(M_ERROR, 0, "Error: servicio invalido!", longitud_respuesta);
}


/*--------------------------------------------------------------------*
 * Inicio de sesion 															                    *
 *--------------------------------------------------------------------*/
void * iniciar_sesion(char * mensaje, int * longitud_respuesta) {
	INICIAR_SESION *iniciar_sesion = (INICIAR_SESION *) mensaje;
	USUARIO * usuario;
	int numero_sesion = 0;

	usuario = buscar_usuario(iniciar_sesion->usuario);

	// Si el usuario no existe
	if(usuario == NULL){
		printf("Servidor UDP: Error: El usuario %s no existe!\n", iniciar_sesion->usuario);
		return mensaje_error(M_ERROR, 0, "Error: El usuario especificado no existe!", longitud_respuesta);
	}
	// printf("Usuario encontrado: %s y %s\n", usuario->usuario, usuario->clave);

	//Si la contraseña es incorrecta
	if(strcmp(iniciar_sesion->clave, usuario->clave) != 0){
		printf("Servidor UDP: Error: El usuario %s ha ingresado una contraseña no valida.\n", usuario->usuario);
		return mensaje_error(M_ERROR, 0,"Error: Contraseña incorrecta!", longitud_respuesta);		}
	// Si no hubo error se inicia la sesion.
	numero_sesion = buscar_sesion_por_usuario(usuario->usuario);
	if(numero_sesion < 0){
		numero_sesion = asignar_numero_sesion();
		agregar_sesion(usuario->usuario, numero_sesion);
	}
	printf("Servidor UDP: El usuario %s ha iniciado sesion.\n", usuario->usuario);
	return mensaje_confirmacion(M_CONFIRMAR ,numero_sesion, 0, "Sesion iniciada correctamente!" , longitud_respuesta);
}


/*--------------------------------------------------------------------*
 * Registro de usuarios														                    *
 *--------------------------------------------------------------------*/
void * registrar(char * mensaje, int * longitud_respuesta) {
	REGISTRO *registro = (REGISTRO *) mensaje;
	USUARIO * usuario;
	BOOLEAN usuario_esta_agregado;

	usuario = buscar_usuario(registro->usuario);

	// Si el usuario ya existe , no debe crearlo
	if(usuario != NULL){
			printf("Servidor UDP: Error: El usuario %s ya existe, no se puede registrar nuevamente.\n", registro->usuario);
			return mensaje_error(M_ERROR, 0,"Error: El usuario especificado ya existe!", longitud_respuesta);
	}
	// Intenta agregar al usuario.
	usuario_esta_agregado = agregar_usuario(registro->usuario, registro->clave, registro->nombre_completo, registro->apellido);
	// Si el archivo no pudo ser accedido.
	if( !usuario_esta_agregado ){
		printf("Servidor UDP: Error: No se ha podido registrar el usuario dentro del archivo.\n");
		return mensaje_error(M_ERROR, 0,"Error: El archivo usuarios.txt no pudo ser abierto.", longitud_respuesta);
	}
	if(!crear_carpeta_usuario(registro->usuario)){
		printf("Servidor UDP: Error: No se pudo crear la carpeta para guardar los albumes del usuario.\n");
		return mensaje_error(M_ERROR, 0,"Error: La carpeta de albumes para el usuario no pudo ser creada.", longitud_respuesta);
	}
	if(!crear_archivos_compartir(registro->usuario)){
		printf("Servidor UDP: Error: No se han podido crear los archivos para compartir.\n");
		return mensaje_error(M_ERROR, 0,"Error: No se han podido crear los archivos para compartir.", longitud_respuesta);		
	}

	printf("Servidor UDP: Se ha registrado un nuevo usuario llamado: %s %s.\n", registro->nombre_completo, registro->apellido);
	return mensaje_confirmacion(M_CONFIRMAR , 0, 0, "Usuario registrado correctamente!" , longitud_respuesta);
}


/*--------------------------------------------------------------------*
 * Se ocupa de resolver las sub-operaciones del cliente               *
 *--------------------------------------------------------------------*/
void * solicitud(char * mensaje, int * longitud_respuesta){
	SOLICITUD * solicitud = (SOLICITUD *)mensaje;
	char * usuario;

	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);
	if(usuario == NULL){
		printf("Servidor UDP: Error: El usuario con ID %d no ha podido ser validado.\n", solicitud->ID_Usuario);
		return mensaje_error(M_ERROR , solicitud->ID_SUB_OP,"Error: Usuario invalido.", longitud_respuesta);
	}

	switch(solicitud->ID_SUB_OP){

	case SubOP_Listar_albumes:
		break;
	case SubOP_Crear_album:
		return subOP_crear_album(solicitud,longitud_respuesta);
		break;
	case SubOP_Modificar_album:
		return modificar_album(solicitud, longitud_respuesta);
		break;
	case SubOP_Eliminar_album:
		return subOP_eliminar_album( solicitud,longitud_respuesta );
		break;
	case SubOP_Listar_archivos_album:
		break;
	case SubOP_Subir_archivo_album:
		break;
	case SubOP_Modificar_archivo_album:
		return modificar_archivo(solicitud, longitud_respuesta);
		break;
	case SubOP_Eliminar_archivo_album:
		return subOP_eliminar_archivo(solicitud, longitud_respuesta);
		break;
	case SubOP_Compartir_album_usuario:
		return subOP_compartir_album_usuario(solicitud, longitud_respuesta);
		break;
	case SubOP_Dejar_compartir_album_usuario:
		return subOP_dejar_compartir_album_usuario(solicitud, longitud_respuesta);
		break;
	case SubOP_Listar_usuarios:
		break;
	}
	printf("Servidor UDP: Usuario %s: Error: El codigo de sub operacion %d no ha sido reconocido.\n", usuario, solicitud->ID_SUB_OP);
	return mensaje_error(M_ERROR, solicitud->ID_SUB_OP,"Error: El codigo de sub operacion no existe", longitud_respuesta);
}


/*--------------------------------------------------------------------*
 * Cerrar sesion de usuario 											                    *
 *--------------------------------------------------------------------*/
void * cerrar_sesion(char * mensaje, int * longitud_respuesta) {
	CERRAR_SESION *cerrar_sesion = (CERRAR_SESION *) mensaje;
	BOOLEAN usuario_cerro_sesion;
	char * nombre_usuario;

	nombre_usuario = buscar_usuario_por_sesion(cerrar_sesion->ID_Usuario);
	usuario_cerro_sesion = cerrar_sesion_usuario(cerrar_sesion->ID_Usuario);

	// Si el usuario no pudo cerrar sesion
	if(!usuario_cerro_sesion){
		printf("Servidor UDP: Usuario %s: Error: La sesion del usuario con ID %d no ha podido cerrarse.\n", nombre_usuario, cerrar_sesion->ID_Usuario);
		return mensaje_error(M_ERROR , 0,"Error: No se pudo cerrar la sesion", longitud_respuesta);
	}
	printf("Servidor UDP: El usuario %s ha cerrado sesion.\n", nombre_usuario);
	return mensaje_confirmacion(M_CONFIRMAR, 0, 0, "Sesion cerrada correctamente!", longitud_respuesta);
}

/*--------------------------------------------------------------------*
 * Crear un album 							 								 	                   *
 *--------------------------------------------------------------------*/
void * subOP_crear_album( SOLICITUD * solicitud , int * longitud_respuesta ){
	char * nombre_usuario;
	nombre_usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);
		// TODO validacion de errorer de crear_album
		if(crear_album(solicitud->nombre, nombre_usuario)){
			// TODO validacion de errores de registrar_album
			if(registrar_album(solicitud->nombre, nombre_usuario)){
				printf("Servidor UDP: El usuario %s ha creado un nuevo album llamado %s\n", nombre_usuario, solicitud->nombre);
				return mensaje_confirmacion(M_CONFIRMAR ,solicitud->ID_Usuario, solicitud->ID_SUB_OP, "Album creado correctamente!", longitud_respuesta);
			}
			printf("Servidor UDP: Usuario %s: Error: El album %s no pudo ser registrado en el archivo.\n", nombre_usuario, solicitud->nombre);
			return mensaje_error(M_ERROR, solicitud->ID_SUB_OP,"Error: No se pudo registrar el nuevo album.", longitud_respuesta);
		}
		printf("Servidor UDP: Usuario %s: Error: El album %s no pudo ser creado.\n", nombre_usuario, solicitud->nombre);
		return mensaje_error(M_ERROR, solicitud->ID_SUB_OP,"Error: No se pudo crear el album.", longitud_respuesta);
}

void * subOP_eliminar_album( SOLICITUD * solicitud,int * longitud_respuesta){
	char * nombre_usuario;
	nombre_usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);
	printf("album_id : %d\n",  solicitud->ID_Album);
	char * nombre_album = buscar_album_id(nombre_usuario, solicitud->ID_Album);

	if(eliminar_album(nombre_album, nombre_usuario, solicitud->ID_Album) ){
		printf("Servidor UDP: El usuario %s ha eliminado el album %s\n", nombre_usuario, nombre_album);
		return mensaje_confirmacion(M_CONFIRMAR, solicitud->ID_Usuario, solicitud->ID_SUB_OP, "Album borrado correctamente!", longitud_respuesta);
	}
	printf("Servidor UDP: Usuario %s: Error: El album %s no pudo ser eliminado.\n", nombre_usuario, nombre_album);
	return mensaje_error(M_ERROR , solicitud->ID_SUB_OP,"Error: No se pudo borrar el album", longitud_respuesta);
}


void * modificar_album(SOLICITUD * solicitud, int * longitud_respuesta){
	CONFIRMAR * mensaje_confirmacion;
	ERROR * mensaje_error;

	char * nombre_actual;
	char * usuario;

	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);

	nombre_actual = buscar_album_id(usuario, solicitud->ID_Album);

	mensaje_error = (ERROR *)malloc(sizeof(ERROR));
	if(renombrar_album(usuario, nombre_actual, solicitud->nombre)){
		if(renombrar_album_registro(usuario, solicitud->ID_Album, solicitud->nombre)){
			printf("Servidor UDP: El usuario %s ha cambiado el nombre del album %s por %s\n", usuario, nombre_actual, solicitud->nombre);
			mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

			mensaje_confirmacion->OP = M_CONFIRMAR;
			mensaje_confirmacion->ID_Usuario = solicitud->ID_Usuario;
			mensaje_confirmacion->ID_SUB_OP = solicitud->ID_SUB_OP;
			strcpy(mensaje_confirmacion->mensaje, "El album ha sido renombrado correctamente!");

			*longitud_respuesta = sizeof(CONFIRMAR);

			return (void *)mensaje_confirmacion;
		}
		else{
			printf("Servidor UDP: Usuario %s: Error: No se pudo renombrar el album %s en el archivo de registro.\n", usuario, nombre_actual);
			strcpy(mensaje_error->mensaje, "Error al renombrar el album en el registro.");	
		}
	}
	else{
		printf("Servidor UDP: Usuario %s: Error: El album %s no pudo ser renombrado.\n", usuario, nombre_actual);
		strcpy(mensaje_error->mensaje, "Error al renombrar el album solicitado.");
	}
	
	mensaje_error->OP = M_ERROR;
	mensaje_error->ID_SUB_OP_Fallo = solicitud->ID_SUB_OP;
	*longitud_respuesta = sizeof(ERROR);

	return (void *)mensaje_error;
}

void * subOP_eliminar_archivo( SOLICITUD * solicitud,int * longitud_respuesta){
	char * usuario;
	char * album;
	char * archivo;
	
	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);
	album = buscar_album_id(usuario, solicitud->ID_Album);
	archivo = buscar_archivo_id(usuario, album, solicitud->ID_Archivo);

	if(eliminar_archivo(archivo, usuario, album)){
		if(eliminar_archivo_de_lista(usuario, album, solicitud->ID_Archivo)){
			printf("Servidor UDP: El usuario %s ha eliminado el archivo %s del album %s\n", usuario, archivo, album);
			return mensaje_confirmacion(M_CONFIRMAR, solicitud->ID_Usuario, solicitud->ID_SUB_OP, "Archivo borrado correctamente!", longitud_respuesta);
		}
	}
	printf("Servidor UDP: Usuario %s: Error: El archivo %s no pudo ser eliminado.\n", usuario, archivo);
	return mensaje_error(M_ERROR , solicitud->ID_SUB_OP,"Error: No se pudo borrar el archivo", longitud_respuesta);
}

void * modificar_archivo(SOLICITUD * solicitud, int * longitud_respuesta){
	CONFIRMAR * mensaje_confirmacion;
	ERROR * mensaje_error;

	char * nombre_actual;
	char * usuario;
	char * album;
	char * extension;
	char * nombre_nuevo;

	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);

	album = buscar_album_id(usuario, solicitud->ID_Album);

	nombre_actual = buscar_archivo_id(usuario, album, solicitud->ID_Archivo);

	extension = strrchr(nombre_actual,'.');

	nombre_nuevo = (char *)malloc(sizeof(char) * strlen(extension) + strlen(solicitud->nombre));
	sprintf(nombre_nuevo, "%s%s", solicitud->nombre, extension);

	mensaje_error = (ERROR *)malloc(sizeof(ERROR));
	if(renombrar_archivo(usuario, album, nombre_actual, nombre_nuevo)){
		if(renombrar_archivo_registro(usuario, album, solicitud->ID_Archivo, nombre_nuevo)){
			printf("Servidor UDP: El usuario %s ha cambiado el nombre del archivo %s por %s\n", usuario, nombre_actual, nombre_nuevo);
			mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

			mensaje_confirmacion->OP = M_CONFIRMAR;
			mensaje_confirmacion->ID_Usuario = solicitud->ID_Usuario;
			mensaje_confirmacion->ID_SUB_OP = solicitud->ID_SUB_OP;
			strcpy(mensaje_confirmacion->mensaje, "El archivo ha sido renombrado correctamente!");

			*longitud_respuesta = sizeof(CONFIRMAR);

			return (void *)mensaje_confirmacion;
		}
		else{
			printf("Servidor UDP: Usuario %s: Error: El archivo %s no pudo ser renombrado en el archivo de registro.\n", usuario, nombre_actual);
			strcpy(mensaje_error->mensaje, "Error al renombrar el archivo en el registro.");	
		}
	}
	else{
		printf("Servidor UDP: Usuario %s: Error: El archivo %s no pudo ser renombrado.\n", usuario, nombre_actual);
		strcpy(mensaje_error->mensaje, "Error al renombrar el archivo solicitado.");
	}
	
	mensaje_error->OP = M_ERROR;
	mensaje_error->ID_SUB_OP_Fallo = solicitud->ID_SUB_OP;
	*longitud_respuesta = sizeof(ERROR);

	return (void *)mensaje_error;
}

void * subOP_compartir_album_usuario(SOLICITUD * solicitud, int * longitud_respuesta){
	CONFIRMAR * mensaje_confirmacion;
	ERROR * mensaje_error;

	char * usuario_destino;
	char * usuario;
	char * album;

	usuario_destino = (char *)malloc(sizeof(char) * strlen(solicitud->nombre));
	strcpy(usuario_destino, solicitud->nombre);

	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);

	album = buscar_album_id(usuario, solicitud->ID_Album);

	if(compartir_album_usuario(usuario, usuario_destino, album, solicitud->ID_Album)){
		printf("Servidor UDP: El usuario %s ha compartido el album %s con el usuario %s\n", usuario, album, usuario_destino);
		mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

		mensaje_confirmacion->OP = M_CONFIRMAR;
		mensaje_confirmacion->ID_Usuario = solicitud->ID_Usuario;
		mensaje_confirmacion->ID_SUB_OP = solicitud->ID_SUB_OP;
		strcpy(mensaje_confirmacion->mensaje, "El album ha sido compartido correctamente!");

		*longitud_respuesta = sizeof(CONFIRMAR);

		return (void *)mensaje_confirmacion;
	}
	printf("Servidor UDP: Usuario %s: Error: El album %s no pudo ser compartido con el usuario %s.\n", usuario, album, usuario_destino);

	mensaje_error = (ERROR *)malloc(sizeof(ERROR));

	mensaje_error->OP = M_ERROR;
	mensaje_error->ID_SUB_OP_Fallo = solicitud->ID_SUB_OP;
	strcpy(mensaje_error->mensaje, "Error al compartir el album especificado.");
		
	*longitud_respuesta = sizeof(ERROR);

	return (void *)mensaje_error;
}

void * subOP_dejar_compartir_album_usuario(SOLICITUD * solicitud, int * longitud_respuesta){
	CONFIRMAR * mensaje_confirmacion;
	ERROR * mensaje_error;

	char * usuario_destino;
	char * usuario;
	char * album;

	usuario_destino = (char *)malloc(sizeof(char) * strlen(solicitud->nombre));
	strcpy(usuario_destino, solicitud->nombre);

	usuario = buscar_usuario_por_sesion(solicitud->ID_Usuario);

	album = buscar_album_id(usuario, solicitud->ID_Album);

	if(dejar_compartir_album_usuario(usuario, usuario_destino, album, solicitud->ID_Album)){
		printf("El usuario %s ha dejado de compartir el album %s con el usuario %s\n", usuario, album, usuario_destino);
		mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

		mensaje_confirmacion->OP = M_CONFIRMAR;
		mensaje_confirmacion->ID_Usuario = solicitud->ID_Usuario;
		mensaje_confirmacion->ID_SUB_OP = solicitud->ID_SUB_OP;
		strcpy(mensaje_confirmacion->mensaje, "El album de ha dejado de compartir correctamente!");

		*longitud_respuesta = sizeof(CONFIRMAR);

		return (void *)mensaje_confirmacion;
	}
	printf("Servidor UDP: Usuario %s: Error: El album %s no pudo dejar de ser compartido con el usuario %s.\n", usuario, album, usuario_destino);
	
	mensaje_error = (ERROR *)malloc(sizeof(ERROR));

	mensaje_error->OP = M_ERROR;
	mensaje_error->ID_SUB_OP_Fallo = solicitud->ID_SUB_OP;
	strcpy(mensaje_error->mensaje, "Error al dejar de compartir el album especificado.");
		
	*longitud_respuesta = sizeof(ERROR);

	return (void *)mensaje_error;
}