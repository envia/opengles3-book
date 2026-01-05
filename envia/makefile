.PHONY: all clean

all: hello_triangle

clean:
	rm -f hello_triangle

hello_triangle: hello_triangle.c
	cc hello_triangle.c -lEGL -lGL -lX11 -o hello_triangle
