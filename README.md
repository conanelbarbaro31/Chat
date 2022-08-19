# Chat
Implementación de un chat usando sockets

Para que funcione el chat, se debn establecer direcciones IP estáticas a cada equipo en la red LAN, se debe correr el código una vez para que se cree la base de datos y sus tablas, una vez creadas, hacer los siguientes pasos:

En la tabla de equipo_local se debe agregar la dirección IP y el nombre del equipo que está ejecutando el código, esta se debe editar por cada equipo en la red.
En la tabla equipos_registrados se deben agregar las direcciones IP y los nombres de los equipos que pertenecen a la red y que ejecutarán el chat, el campo puerto de esta tabla no es necesario llenarlo debido a que ya se encuentra seteado por código.
La tabla mensajes no se debe tocar, ahí se almacenarán los mensajes que se envíen y reciban por el chat entre los equipos conectados a la red.

Una vez hecho lo anterior, compartir esta base de datos con el resto de equipos.
