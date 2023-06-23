# TPE Protocolos de comunicación

## Instalación 
Para compilar el proyecto, ubicarse en el directorio con los códigos fuentes y ejecutar
```
    make all CC=gcc
```
O se puede elegir tambien a  _clang_ como compilador
```
    make all CC=clang
```
Luego, se habrán creado 2 ejecutables en la carpeta _bin_, generada en el directorio del proyecto. Para correr el servidor, ejecutar
```
    ./bin/popserver -d <path_a_maildir>
```
Si se quieren consultar algunas opciones que se pueden especificar antes de correrlo, ejecutar
```
    ./bin/popserver -h
```
Para utilizar el cliente, se debe especificar un token de autorizacion. Este puede ser cambiando antes
de ejecutar el servidor, y su valor default es _1234_. Por ejemplo, si se quieren conocer las conexiones
hisóricas al servidor, ejecutar
```
    ./bin/popadmin -p 
```
Allí se le pedira que ingrese el token por entrada estándar. Recuerde que el servidor debe estar ejecutándose
previamente

Los logs se almacenarán en la carpeta _log_, también generada en el directorio del proyecto. Cada archivo será identificado
por el momento en el que empezó a correr el servidor

### Grupo 06
* Axel Facundo Preiti Tasat: https://github.com/AxelPreitiT
* Gastón Ariel Francois: https://github.com/francoisgaston
* José Rodolfo Mentasti: https://github.com/JoseMenta
* Nicolás Suarez Durrels: https://github.com/nicosuarez272