# ============================================================
# Variables de Compilador y Flags
# ============================================================
CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fopenmp -O2 -I$(INCDIR)
LDFLAGS  = -lcurl -lssh2

# ============================================================
# Nombre del Ejecutable Final
# ============================================================
TARGET = farmacia_parallel

# ============================================================
# Directorios de Trabajo
# ============================================================
SRCDIR  = src
INCDIR  = include
OBJDIR  = obj
DATADIR = data

# ============================================================
# Búsqueda Automática de Código Fuente e Identificación de Objetos
# ============================================================
SRCS = main.cpp $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SRCS)))

# ============================================================
# Regla Principal — compilación con OpenMP activo (Tarea 4)
# ============================================================
all: $(DATADIR) $(TARGET)

# ============================================================
# Target opcional: versión secuencial para análisis de speedup (Tarea 8)
# Compila el mismo código con la macro SEQUENCIAL definida.
# Uso: make seq
# ============================================================
seq: CXXFLAGS += -DSEQUENCIAL
seq: $(DATADIR) $(TARGET)

# ============================================================
# Vinculación del Binario Final
# ============================================================
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# ============================================================
# Compilación de main.cpp (raíz del proyecto)
# ============================================================
$(OBJDIR)/main.o: main.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# Compilación de los módulos en src/
# ============================================================
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# Creación de directorios necesarios
# ============================================================
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(DATADIR):
	mkdir -p $(DATADIR)

# ============================================================
# Limpieza de compilación (preserva data/ para no re-descargar CSVs)
# ============================================================
clean:
	rm -rf $(OBJDIR) $(TARGET) resultados.txt log.txt

# ============================================================
# Limpieza total incluyendo los CSV descargados
# ============================================================
distclean: clean
	rm -rf $(DATADIR)

.PHONY: all seq clean distclean