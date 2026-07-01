# Control externo de LEDs

`setcolor` esta pensado para compilarse una vez durante la instalacion y cambiar el
comportamiento durante la ejecucion. Asi no hay que editar `setcolor.c` ni
recompilar para pasar de 10 a 20 LEDs, cambiar brillo o decidir cuales LEDs se
encienden.

## Instalacion vs ejecucion

Durante la instalacion conviene hacer solo esto:

- instalar dependencias;
- compilar `setcolor`;
- crear un archivo de configuracion inicial, si el instalador lo necesita.

Durante la ejecucion conviene cambiar esto:

- cantidad fisica de LEDs;
- GPIO, DMA, brillo y tipo de tira;
- color;
- patron de LEDs encendidos/apagados.

La cantidad debe definirse antes de inicializar la libreria `ws2811`, pero puede
venir desde argumentos o desde un archivo de configuracion.

## Uso rapido

Uso compatible con la version anterior:

```bash
sudo ./setcolor 255 0 0
```

Por defecto, ese modo conserva el comportamiento pedido anteriormente: apaga el
primer y ultimo LED, y pinta los LEDs intermedios.

Uso recomendado con parametros explicitos:

```bash
sudo ./setcolor --count 20 --rgb 255 0 0 --mode skip-ends
```

Encender todos:

```bash
sudo ./setcolor --count 20 --rgb 255 0 0 --mode all
```

Uno si y uno no:

```bash
sudo ./setcolor --count 20 --rgb 255 0 0 --mode alternate
```

Uno no y uno si:

```bash
sudo ./setcolor --count 20 --rgb 255 0 0 --mode alternate-offset
```

Apagar todo:

```bash
sudo ./setcolor --count 20 --off
```

## Seleccion exacta de LEDs

Encender solo algunos indices:

```bash
sudo ./setcolor --count 20 --hex FF0000 --only 1,3,5,7,9
```

Encender un rango:

```bash
sudo ./setcolor --count 20 --hex 00FF00 --only 4-12
```

Encender todos menos algunos:

```bash
sudo ./setcolor --count 20 --hex 0000FF --mode all --off-leds 0,19
```

Usar una mascara:

```bash
sudo ./setcolor --count 10 --hex FF8800 --mask 0111111110
```

En la mascara, cada caracter representa un LED:

```text
0 = apagado
1 = encendido
```

Entonces `0111111110` significa:

```text
LED 0 apagado
LED 1 encendido
...
LED 8 encendido
LED 9 apagado
```

La longitud de `--mask` debe ser igual a `--count`.

## Archivo de configuracion

Un instalador bash puede crear un archivo como `leds.conf`:

```ini
count=20
gpio=18
dma=10
brightness=180
invert=false
strip_type=GRB
mode=skip-ends
# opcional: mask=01111111111111111110
# opcional: off_leds=0,19
```

Luego se ejecuta asi:

```bash
sudo ./setcolor --config leds.conf --hex FF0000
```

Los argumentos de ejecucion pueden sobreescribir el archivo:

```bash
sudo ./setcolor --config leds.conf --count 30 --mode alternate --hex 00FF00
```

## Opciones soportadas

```text
--config FILE       Lee configuracion key=value
--count N           Cantidad fisica de LEDs
--gpio N            GPIO de salida
--dma N             Canal DMA
--brightness N      Brillo 0-255
--invert            Invierte la salida
--strip TYPE        RGB, RBG, GRB, GBR, BRG, BGR, RGBW o GRBW
--rgb R G B         Color RGB 0-255
--hex RRGGBB        Color hexadecimal
--off               Apaga todos los LEDs
--mode MODE         all, skip-ends, alternate, alternate-offset
--mask 0101...      Mascara exacta de LEDs
--only LIST         Enciende solo indices/rangos
--off-leds LIST     Apaga indices/rangos despues del modo/mask
```

## Compilacion

Con SCons:

```bash
scons
```

Con CMake:

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```
