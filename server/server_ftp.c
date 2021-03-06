#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "../utils/definitions.h"
#include "../utils/ftp_utils.h"
#include "album.h"
#include "archivo.h"
#include "sesion.h"
#include "usuario.h"
#include "server_ftp.h"

BOOLEAN iniciar_servidor_ftp(int puerto){

  int sockid, newsd, pid, clilen = 0;
  struct sockaddr_in my_addr, client_addr;

  printf("Servidor TCP: Creando socket\n");
  if ((sockid = socket(AF_INET,SOCK_STREAM,0)) < 0){
    printf("Servidor TCP: Error creando el socket: %d\n", errno);
    exit(0);
  }

  printf("Servidor TCP: Creando enlace con socket local\n");
  bzero((char *) &my_addr,sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(puerto);
  my_addr.sin_addr.s_addr = htons(INADDR_ANY);
  if (bind(sockid ,(struct sockaddr *) &my_addr,sizeof(my_addr)) < 0){
    printf("Servidor TCP: Error al crear el enlace: %d\n", errno);
    exit(0);
  }

  printf("Servidor TCP: Escuchando\n");
  if (listen(sockid,5) < 0){
    printf("Servidor TCP: Error al intentar escuchar: %d\n",errno);
    exit(0);
  }

  while(1==1) {
    /* ACCEPT A CONNECTION AND THEN CREATE A CHILD TO DO THE WORK */
    /* LOOP BACK AND WAIT FOR ANOTHER CONNECTION                  */
    printf("Servidor TCP: Esperando conexion\n");
    if((newsd = accept(sockid ,(struct sockaddr *) &client_addr,&clilen)) < 0){
      printf("Servidor TCP: Error de conexion: %d\n", errno);
      exit(0);
    }
    printf("Servidor TCP: Conexion establecida, el socket asignado es el: %d\n",newsd);

    if((pid=fork()) == 0) {
      /* CHILD PROC STARTS HERE. IT WILL DO ACTUAL FILE TRANSFER */
      close(sockid);   /* child shouldn't do an accept */
      doftp(newsd);
      close (newsd);
      exit(0);         /* child all done with work */
    }
    /* PARENT CONTINUES BELOW HERE */
    close(newsd);        /* parent all done with client, only child */
  }              /* will communicate with that client from now on */
}


/* CHILD PROCEDURE, WHICH ACTUALLY DOES THE FILE TRANSFER */
void doftp(int newsd){
  char msg[MAXLINEA];
  int longitud;

  SOLICITUD * mensaje_solicitud;
  CONFIRMAR * mensaje_confirmacion;

  char * usuario;
  char * album;
  char * archivo;
  char * ruta;

  // Se espera un mensaje de solicitud que proporcione el nombre del achivo, el album
  // y el usuario al que pertenece para determinar el destino del archivo a recibir.

  if((read(newsd,(char *)msg,MAXLINEA)) < 0){
    printf("Servidor TCP: Error de lectura: %d\n",errno);
    exit(0);
  }

  if(msg[0] == M_SOLICITUD){
    mensaje_solicitud = (SOLICITUD *)msg;
    
    // Obtener nombre usuario.
    usuario = buscar_usuario_por_sesion(mensaje_solicitud->ID_Usuario);
    // Obtener nombre del album.
    album = buscar_album_id(usuario, mensaje_solicitud->ID_Album);

    switch(mensaje_solicitud->ID_SUB_OP){
    case SubOP_Subir_archivo_album:
      recepcion_archivo(newsd, mensaje_solicitud);
      break;
    case SubOP_Descargar_archivo_album:
      envio_archivo(newsd, mensaje_solicitud);
      break;
    case SubOP_Listar_albumes:
      listar_albumes_usuario(newsd, mensaje_solicitud);
      break;
    case SubOP_Listar_archivos_album:
      listar_archivos_album_usuario(newsd, mensaje_solicitud);
      break;
    case SubOP_Listar_usuarios:
      listar_usuarios_registrados(newsd, mensaje_solicitud);
      break;
    case SubOP_Listar_albumes_compartidos_conmigo:
      listar_albumes_compartidos_usuario(newsd, mensaje_solicitud);
      break;
    case SubOP_Listar_albumes_compartidos_otros:
      listar_albumes_compartidos_otros(newsd, mensaje_solicitud);
      break;
    }
  }
}

void recepcion_archivo(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char * album;
  char * archivo;
  char * ruta;
  char * aux;
  char * aux1;
  int longitud_cadena = 0;

  // Obtener nombre usuario.
  usuario = strtok(mensaje_solicitud->nombre,";");

  // Obtener nombre de archivo.
  archivo = strtok(NULL,";");
  
  // Obtener nombre del album.
  album = buscar_album_id(usuario, mensaje_solicitud->ID_Album);

  // archivo = (char *)malloc(sizeof(char) * strlen(mensaje_solicitud->nombre));
  // strcpy(archivo, mensaje_solicitud->nombre);

  // Armar ruta completa del archivo a almacenar.
  ruta = crear_ruta(usuario, album, archivo);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  if(recibir_archivo_socket("Servidor TCP", socket_id, ruta)){
    registrar_archivo(archivo, usuario, album);
  }
}

void envio_archivo(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char * album;
  char * archivo;
  char * ruta;

  // Obtener nombre usuario.
  usuario = (char *)malloc(strlen(mensaje_solicitud->nombre));
  strcpy(usuario, mensaje_solicitud->nombre);
  
  // Obtener nombre del album.
  album = buscar_album_id(usuario, mensaje_solicitud->ID_Album);

  // Obtener nombre del archivo.
  archivo = buscar_archivo_id(usuario, album, mensaje_solicitud->ID_Archivo);

  // Armar ruta completa del archivo a almacenar.
  ruta = crear_ruta(usuario, album, archivo);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, archivo);

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  enviar_archivo_socket("Servidor TCP", socket_id, ruta);
}

void listar_albumes_usuario(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char archivo_temporal[MAXPATH];
  FILE * fp;
  int i = 1;

  // Obtener nombre usuario.
  usuario = buscar_usuario_por_sesion(mensaje_solicitud->ID_Usuario);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i++, EXTENSION_ARCHIVO_TEMPORAL);
  while((fp = fopen(archivo_temporal, "w")) == NULL){
    sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i, EXTENSION_ARCHIVO_TEMPORAL);
    i++;
  }

  listar_albumes(fp, usuario);

  enviar_archivo_socket("Servidor TCP", socket_id, archivo_temporal);

  remove(archivo_temporal);
}

void listar_archivos_album_usuario(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char * album;
  char archivo_temporal[MAXPATH];
  FILE * fp;
  int i = 1;

  // Obtener nombre usuario.
  usuario = buscar_usuario_por_sesion(mensaje_solicitud->ID_Usuario);

  // Obtener nombre del album.
  album = buscar_album_id(usuario, mensaje_solicitud->ID_Album);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i++, EXTENSION_ARCHIVO_TEMPORAL);
  while((fp = fopen(archivo_temporal, "w")) == NULL){
    sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i, EXTENSION_ARCHIVO_TEMPORAL);
    i++;
  }

  listar_archivos(fp, usuario, album);

  enviar_archivo_socket("Servidor TCP", socket_id, archivo_temporal);

  remove(archivo_temporal);
}

void listar_usuarios_registrados(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char archivo_temporal[MAXPATH];
  FILE * fp;
  int i = 1;

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i++, EXTENSION_ARCHIVO_TEMPORAL);
  while((fp = fopen(archivo_temporal, "w")) == NULL){
    sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i, EXTENSION_ARCHIVO_TEMPORAL);
    i++;
  }

  listar_usuarios(fp);

  enviar_archivo_socket("Servidor TCP", socket_id, archivo_temporal);

  remove(archivo_temporal);
}

void listar_albumes_compartidos_usuario(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char archivo_temporal[MAXPATH];
  FILE * fp;
  int i = 1;

  // Obtener nombre usuario.
  usuario = buscar_usuario_por_sesion(mensaje_solicitud->ID_Usuario);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i++, EXTENSION_ARCHIVO_TEMPORAL);
  while((fp = fopen(archivo_temporal, "w")) == NULL){
    sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i, EXTENSION_ARCHIVO_TEMPORAL);
    i++;
  }

  listar_albumes_compartidos_conmigo(fp, usuario);

  enviar_archivo_socket("Servidor TCP", socket_id, archivo_temporal);

  remove(archivo_temporal);
}

void listar_albumes_compartidos_otros(int socket_id, SOLICITUD * mensaje_solicitud){
  CONFIRMAR * mensaje_confirmacion;
  char * usuario;
  char archivo_temporal[MAXPATH];
  FILE * fp;
  int i = 1;

  // Obtener nombre usuario.
  usuario = buscar_usuario_por_sesion(mensaje_solicitud->ID_Usuario);

  mensaje_confirmacion = (CONFIRMAR *)malloc(sizeof(CONFIRMAR));

  mensaje_confirmacion->OP = M_CONFIRMAR;
  mensaje_confirmacion->ID_Usuario = mensaje_solicitud->ID_Usuario;
  mensaje_confirmacion->ID_SUB_OP = mensaje_solicitud->ID_SUB_OP;
  strcpy(mensaje_confirmacion->mensaje, "OK");

  // Se envia un mensaje al cliente aceptando el archivo y validando los datos.
  if((write(socket_id,(void *)mensaje_confirmacion,sizeof(CONFIRMAR))) < 0){
    printf("Servidor TCP: Error al enviar el mensaje de confirmacion de datos: %d\n",errno);
    exit(0);
  }

  sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i++, EXTENSION_ARCHIVO_TEMPORAL);
  while((fp = fopen(archivo_temporal, "w")) == NULL){
    sprintf(archivo_temporal, "%s%d%s", ARCHIVO_TEMPORAL_BASE, i, EXTENSION_ARCHIVO_TEMPORAL);
    i++;
  }

  listar_albumes_compartidos_otro(fp, usuario);

  enviar_archivo_socket("Servidor TCP", socket_id, archivo_temporal);

  remove(archivo_temporal);
}