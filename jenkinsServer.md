Guía de Instalación: Despliegue de Jenkins en Local mediante Docker para CI/CD

El objetivo de esta guía es configurar un servidor Jenkins en modo local usando Docker. Esta arquitectura aísla el servidor, asegura su portabilidad y evita muchos problemas de red y permisos corporativos. A continuación se presentan los pasos ordenados para montar el sistema por completo.

PASO 1: Instalación de Docker y Configuración Base
El primer paso es preparar nuestra máquina virtual instalando el motor que ejecutará nuestro servidor Jenkins.

Abre tu terminal de Ubuntu.
Actualiza tu lista de programas ejecutando el comando: sudo apt update
Instala Docker y sus herramientas asociadas ejecutando: sudo apt install docker.io docker-compose -y
PASO 2: Creación de la Estructura de Carpetas
Para que las configuraciones de nuestro servidor Jenkins y su historial no se borren accidentalmente si reiniciamos la máquina, necesitamos asignarle un almacenamiento persistente.

Asegúrate de estar en tu carpeta de usuario ejecutando: cd ~
Crea una carpeta principal para el servidor y entra en ella mediante: mkdir jenkins-local && cd jenkins-local
Crea la subcarpeta que guardará las contraseñas y datos internos de Jenkins: mkdir jenkins_data
Aplica los permisos necesarios para que el motor interno (que usa el grupo de sistema "1000") pueda guardar archivos allí ejecutando: sudo chown -R 1000:1000 jenkins_data
PASO 3: Definición de la Infraestructura
Ahora debemos redactar un archivo de texto con las "instrucciones de montaje" que le indicarán a Docker cómo debe lanzar y conectar nuestro contenedor.

Manteniéndote dentro de tu carpeta jenkins-local, copia y pega todo el siguiente bloque de texto en tu terminal y pulsa Enter. Esto creará el archivo de arquitectura.
cat << 'EOF' > docker-compose.yml
version: '3'
services:
jenkins:
image: jenkins/jenkins:lts
container_name: mi-jenkins
ports:
- "8080:8080"
- "50000:50000"
volumes:
- ./jenkins_data:/var/jenkins_home
restart: always
EOF

PASO 4: Arranque y Extracción de Claves
Con todas las piezas en su sitio, es momento de encender el servidor y extraer su contraseña nativa de seguridad.

Enciende el contenedor Jenkins de forma silenciosa para que se quede trabajando en segundo plano usando: sudo docker-compose up -d
Extrae la clave de seguridad secreta que Jenkins ha generado para su primer arranque ejecutando: sudo docker logs mi-jenkins
Lee el texto que el sistema arroja en pantalla, localiza la frase "Please use the following password to proceed to installation" y copia la larga cadena de números y letras que verás justo debajo de ese aviso.
PASO 5: Desbloqueo y Configuración en el Navegador
Por último, entraremos en la interfaz gráfica del servidor para terminar de armarlo y dejarlo listo para su conexión con Git.

Abre tu navegador web como si fueras a buscar algo en Internet.
En la barra superior, escribe la dirección de tu entorno local: http://localhost:8080
La web te pedirá la contraseña para desbloquear la pantalla inicial. Pega ahí la clave larga que copiaste en el Paso 4.
Tras validarla, pulsa en el botón para instalar los "Plugins sugeridos" (Install suggested plugins). Jenkins instalará entonces todo el soporte para ramas y configuraciones remotas.
Tras unos minutos cargando herramientas, rellena el formulario que te aparece para crear tu propio usuario administrador.
Acepta el cuadro final que confirma la URL del servidor y accede al panel principal (Dashboard).
El sistema Jenkins ya se encuentra puramente operativo y establecen local a la espera de ser enlazado con el repositorio de código.
