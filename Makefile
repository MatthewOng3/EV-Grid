# Variables
CC=mpicc
CFLAGS=-fopenmp
SRC= logging.c node.c util.c main.c
OUT=Output

# Targets and Recipes
all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run: $(OUT)
	mpirun -np 10 --oversubscribe ./$(OUT) 3 3

clean:
	rm -f $(OUT)
