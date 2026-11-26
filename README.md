# Mis finanzas

App local para apuntar gastos e ingresos y verlos por mes. C++ con Crow, SQLite y HTML + HTMX/Chart.js.

## Arrancar

Necesitas CMake (≥ 3.23, por los presets), Conan 2 y un compilador C++17. Crow, Asio y SQLite3 se gestionan con Conan; HTMX y Chart.js se sirven como ficheros estáticos locales.

```sh
conan profile detect          # solo la primera vez
conan install . --output-folder=build --build=missing
conan install . --output-folder=build --build=missing -s build_type=Debug   # solo si usas el preset debug
cmake --preset release
cmake --build --preset release
./build/release/finances
```

Abre <http://127.0.0.1:8080>. El ejecutable crea `finances.db` en el directorio desde el que lo lances, así que arráncalo desde aquí. CMake genera `build/release/compile_commands.json` (o `build/debug/` si usas el preset `debug`).

## Uso

- **Dashboard**: balance del mes actual, evolución de los últimos 6 meses y gastos por categoría.
- **Este mes / Otros meses**: listado ordenado por fecha de inserción (más reciente primero), con sus estadísticas y un formulario para añadir movimientos. El formulario actualiza la lista y las estadísticas sin recargar la página (HTMX).
- Cada movimiento tiene título, importe, tipo (gasto/ingreso), categoría (Alimentación, Transporte, Vivienda, Ocio, Otros) y fecha. El importe acepta coma o punto decimal.

## Prueba

```sh
ctest --preset release
```

Levanta el servidor con una base de datos temporal y comprueba el dashboard, el alta, la validación y los totales.
