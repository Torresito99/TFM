# Estudio de la autonomía energética del prototipo de detector de caídas

## 1. Propósito del estudio

Uno de los requisitos fundamentales de un dispositivo destinado a la detección de caídas en
personas mayores es que pueda llevarse puesto de forma continua y fiable durante el mayor tiempo
posible sin necesidad de recargas frecuentes. Un sistema de este tipo solo cumple su función si
está encendido y vigilando en el momento en que ocurre la caída; por tanto, cada hora que el
dispositivo pasa cargándose o apagado por falta de batería es una hora durante la cual el usuario
queda desprotegido. Además, en el caso concreto de las personas mayores, no puede asumirse que
vayan a recordar cargar el dispositivo a diario, por lo que una autonomía amplia no es una mejora
opcional, sino una condición de seguridad.

Por este motivo se consideró imprescindible conocer, de forma empírica y no meramente estimada,
cuánto tiempo es capaz de funcionar el prototipo con una única carga de su batería. El presente
estudio describe cómo se planteó esa medición, por qué se diseñó de la manera en que se hizo y qué
conclusiones se extraen de los resultados obtenidos.

## 2. El compromiso entre detección continua y ahorro de energía

El diseño del firmware parte de una tensión de fondo que conviene explicar, ya que condiciona todo
lo demás. Por un lado, para no perder ninguna caída, el dispositivo debe analizar el movimiento de
forma prácticamente ininterrumpida. Por otro lado, mantener el procesador y los sensores siempre
activos es precisamente lo que más energía consume. El reto del modo de bajo consumo —denominado
internamente «Plan C»— consistió en reducir el gasto energético todo lo posible sin renunciar a la
capacidad de vigilar de manera permanente.

Para lograr ese equilibrio se adoptaron varias medidas, cada una con una justificación concreta:

- **Reducción de la velocidad del procesador.** Se rebajó la frecuencia de la CPU a la mitad de su
  valor habitual. El algoritmo de detección no necesita una gran potencia de cálculo, de modo que
  trabajar a menor velocidad apenas afecta a su funcionamiento pero sí disminuye el consumo.

- **Micro-suspensiones del procesador entre lecturas.** En lugar de mantener la CPU trabajando sin
  descanso, el sistema la duerme durante breves instantes entre una lectura del sensor de movimiento
  y la siguiente. Así se aprovechan los pequeños huecos de inactividad que existen entre muestras
  para ahorrar energía.

- **Apagado de la pantalla en reposo.** La pantalla AMOLED es uno de los componentes más consumidores
  del dispositivo. Dado que, durante el uso normal, el usuario apenas la mira, se decidió mantenerla
  apagada de forma predeterminada y encenderla únicamente cuando se toca. La vigilancia de caídas no
  depende en absoluto de que la pantalla esté encendida.

- **Comunicaciones inalámbricas desactivadas.** Las radios Wi-Fi y Bluetooth consumen una cantidad
  notable de energía cuando están activas. Por ello permanecen apagadas durante el funcionamiento
  normal y solo se activa la baliza Bluetooth de aviso en el preciso instante en que se confirma una
  caída, que es el único momento en que esa comunicación resulta necesaria.

El conjunto de estas decisiones define un escenario de funcionamiento realista: el dispositivo
permanece atento a posibles caídas de forma continua, con la pantalla apagada y sin comunicaciones,
que es exactamente la situación en la que pasaría la inmensa mayoría del tiempo durante un uso real.

## 3. Planteamiento de la medición y justificación del método

Medir la autonomía de un dispositivo de este tipo no es tan sencillo como parece, porque la forma
más cómoda de observar su comportamiento —mantenerlo conectado al ordenador por cable— altera el
propio resultado: mientras está conectado, el dispositivo recibe alimentación externa y no consume
de su batería, por lo que cualquier medida tomada en esas condiciones sería falsa. Era necesario,
por tanto, idear un método que permitiera registrar el consumo real **sin ningún cable conectado**
durante toda la prueba.

La solución adoptada consistió en dotar al propio dispositivo de la capacidad de registrar su estado
de forma autónoma. El planteamiento, y la razón de cada decisión, fue el siguiente:

- El dispositivo anota periódicamente su nivel de batería y su tensión, leyéndolos directamente del
  circuito de gestión de energía que incorpora la placa. De este modo no se depende de ningún
  instrumento externo ni de mantenerlo conectado.

- Esas anotaciones se guardan en la memoria interna no volátil del microcontrolador. La elección de
  esta memoria es deliberada: a diferencia de la memoria normal, conserva la información aunque el
  dispositivo se quede sin batería y se apague por completo. Esto es esencial, ya que el objetivo es
  precisamente medir hasta el momento del agotamiento total.

- La medición se diseñó para iniciarse y detenerse sola. El registro arranca automáticamente en el
  instante en que se desconecta el cable de alimentación —el punto de partida natural de la prueba— y
  continúa sin intervención hasta que la batería se agota. Esta automatización buscaba que el
  procedimiento fuera lo más sencillo posible de ejecutar y evitar que una manipulación manual
  introdujera errores.

- Por último, una vez agotada la batería, basta con recargar el dispositivo y volver a conectarlo
  para que vuelque todo el histórico acumulado y poder analizarlo.

Gracias a este planteamiento, la medición refleja el consumo genuino del dispositivo funcionando por
sí mismo, sin interferencias de la alimentación externa, y resulta además sencilla y repetible.

## 4. Resultados obtenidos

La prueba se desarrolló sobre el prototipo equipado con su batería de 450 mAh, partiendo de una
carga prácticamente completa y dejándolo funcionar de forma ininterrumpida en el modo de bajo
consumo descrito hasta su apagado por agotamiento.

El dispositivo se mantuvo operativo durante **aproximadamente veinticuatro horas**, es decir, en
torno a un día completo de funcionamiento continuo con una sola carga. A lo largo de ese tiempo, la
batería se descargó de manera progresiva y muy regular, sin comportamientos anómalos ni caídas
bruscas inesperadas, lo que indica que tanto el dispositivo como la propia medición se comportaron
de forma estable y fiable.

La descarga siguió el patrón característico de las baterías de litio, que conviene interpretar más
allá del dato concreto. Durante la mayor parte del tiempo —aproximadamente las dos terceras partes
de la autonomía— la batería se consumió de forma lenta y constante, manteniendo una tensión alta y
estable. A continuación atravesó una larga fase intermedia en la que la descarga prosiguió de manera
sostenida. Finalmente, ya cerca del agotamiento, la tensión cayó de forma acusada en un intervalo
breve: es la zona final típica de estas baterías y constituye el momento idóneo para avisar al
usuario de que debe recargar el dispositivo antes de que se apague.

Se observó asimismo que el indicador de porcentaje de batería que proporciona el circuito de gestión
de energía no desciende de manera perfectamente uniforme, sino que en la parte baja de la descarga
tiende a estancarse durante unos minutos antes de seguir bajando. Este comportamiento es propio del
método de estimación que emplea dicho circuito y lleva a una conclusión práctica: en niveles bajos
de carga, la tensión de la batería constituye un indicador más fiable de su estado real que el
porcentaje estimado.

## 5. Valoración de los resultados

El estudio permite extraer varias conclusiones relevantes para el proyecto.

En primer lugar, la medición demostró ser **válida y reproducible**. La evolución de la descarga fue
limpia y coherente con el comportamiento físico esperado de una batería de litio, lo que aporta
confianza tanto en el dato de autonomía obtenido como en el método de medición diseñado, que podrá
reutilizarse para evaluar futuras versiones del firmware.

En segundo lugar, y desde un punto de vista crítico, la autonomía de un día resulta **insuficiente**
frente al objetivo inicial de alcanzar varios días de funcionamiento con una sola carga. Las medidas
de ahorro aplicadas tienen un efecto real y reducen el consumo, pero no consiguen llevarlo al nivel
necesario para ese horizonte de varios días. Conocer esta limitación de forma objetiva es, en sí
mismo, un resultado valioso, ya que orienta con claridad el trabajo de optimización pendiente.

La causa principal de este consumo todavía elevado puede explicarse con sencillez. Aunque el
procesador se duerme entre lectura y lectura del sensor, esas micro-suspensiones son muy breves y se
producen con mucha frecuencia, de manera que, en la práctica, el procesador pasa despierto una parte
considerable del tiempo. A ello se suma que el sensor de movimiento funciona de forma continua. En
otras palabras, la estrategia de ahorro actual reduce el consumo, pero no llega a apagar realmente el
sistema durante los largos periodos en los que no ocurre nada relevante.

## 6. Propuestas de mejora

A partir del diagnóstico anterior, la línea de mejora con mayor potencial no consiste en refinar las
micro-suspensiones actuales, sino en cambiar la filosofía de funcionamiento en reposo. La propuesta
principal es aprovechar la capacidad del propio sensor de movimiento para **avisar por sí mismo
cuando detecta actividad**. De este modo, el procesador podría permanecer en una suspensión profunda
y prolongada durante todo el tiempo en que el usuario está quieto, y despertar únicamente cuando se
produce un movimiento o un impacto que merezca ser analizado. Dado que una persona pasa en reposo la
mayor parte del día, esta estrategia evitaría el grueso del consumo actual y podría incrementar la
autonomía hasta el entorno de varios días, acercándola al objetivo perseguido.

De forma complementaria, podrían apagarse por completo determinados componentes durante el reposo
—en particular el controlador de la pantalla— y reducir la frecuencia de muestreo del sensor mientras
no se detecte actividad, contribuyendo ambos a un menor gasto energético.

---

*Los datos completos registrados durante la prueba se conservan en el fichero
`medida_bateria_planC.csv`, que contiene la evolución del nivel de batería y de la tensión a lo largo
de toda la descarga.*
