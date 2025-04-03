# Proyecto #1: Chat - Servidor

Este sistema de mensajería implementa un servidor en C++ utilizando WebSocket sobre Boost.Beast y Boost.Asio. Permite la conexión de múltiples clientes, manejo de usuarios y comunicación tanto pública como privada, con soporte para cambio de estado, monitoreo de inactividad y almacenamiento de historial de mensajes.

## Características - Servidor

- Comunicación vía WebSocket
- Manejo de múltiples clientes concurrentes
- Mensajes públicos y privados
- Registro de historial de mensajes
- Cambio de disponibilidad de los usuarios (`Disponible`, `Ocupado`, `Ausente`, `Desconectado`)
- Notificaciones de cambios de estado para todos los participantes
- Inactividad detectada automáticamente con cambio a estado `Ausente`
- Al volver a estado `Disponible`, los mensajes pendientes se entregan
- Registro de actividad y errores en archivo de log

## Estructura - Servidor

### Clases principales

- **`Participant`**: Representa a un usuario conectado. Guarda su ID, estado, conexión, historial y mensajes pendientes.
- **`ParticipantRegistry`**: Administra el registro de todos los usuarios conectados. Permite registrar, obtener y actualizar participantes.
- **`CommunicationRepository`**: Almacena el historial de mensajes públicos y privados.
- **`ProtocolUtils`**: Contiene utilidades para construir y parsear mensajes del protocolo entre servidor y cliente.
- **`SystemLogger`**: Maneja el registro de logs a archivo y consola.
- **`ActivityMonitor`**: Verifica la actividad de los usuarios y los marca como `AWAY` si están inactivos cierto tiempo.
- **`RequestHandler`**: Procesa los comandos recibidos por parte de los clientes (pedir lista, cambiar estado, enviar mensajes, etc.).
- **`ConnectionHandler`**: Administra la conexión de cada cliente, autenticación por nombre, recepción de mensajes y desconexión.
- **`MessageSystem`**: Es el punto de entrada del servidor. Inicia el sistema, recibe conexiones, y lanza hilos por cada nuevo cliente.


### Funciones clave

- `register_participant`: Registra un usuario nuevo o reconecta uno que estaba offline.
- `get_participant`: Devuelve el puntero a un participante dado su ID.
- `broadcast`: Envía un mensaje a todos los usuarios conectados.
- `handle_get_participants`: Envía al cliente la lista de usuarios disponibles.
- `handle_set_availability`: Cambia el estado de disponibilidad de un usuario y entrega mensajes pendientes si se activa.
- `handle_send_communication`: Maneja el envío de un mensaje público o privado y lo entrega si es posible.
- `handle_fetch_communications`: Devuelve el historial de mensajes del canal solicitado.
- `update_last_activity`: Actualiza el último momento de actividad del usuario.
- `monitor_loop`: Hilo que detecta inactividad y cambia el estado a `AWAY`.
- `run`: Método principal que escucha nuevas conexiones y crea un hilo por cliente.


### Compilación - Servidor

- g++ -std=c++17 chat_servidor.cpp -o chat_servidor -I/ruta/a/boost -lboost_system -lboost_thread -lpthread
- ./servidor <puerto>
- ./chat_servidor 8080

### Conexión Cliente - Servidor

- IP: 18.188.110.137
- Puerto: 8080


# Sistema de Chat - Cliente 

Este es el cliente gráfico del sistema de mensajería, desarrollado en C++ utilizando **wxWidgets** para la interfaz gráfica, y **Boost.Asio + Boost.Beast** para la comunicación con el servidor mediante el protocolo WebSocket.

Permite a los usuarios conectarse al servidor, gestionar contactos, enviar mensajes públicos y privados, cambiar su estado (activo, ocupado, inactivo), y visualizar el historial de conversaciones.


## Características - Cliente

- Interfaz gráfica con tema oscuro
- Soporte para múltiples contactos y chat general
- Visualización del estado de cada contacto: activo, ocupado, inactivo o desconectado
- Cambio de estado desde la interfaz
- Recepción automática de mensajes entrantes
- Alerta visual de errores o desconexiones
- Reconexión automática ante errores
- Manual de ayuda integrado
