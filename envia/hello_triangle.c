#include <GLES3/gl3.h>
#include <stdlib.h>

GLboolean es_create_window()
{
	return GL_TRUE;
}

int es_main()
{
	es_create_window();
	return GL_TRUE;
}

int main()
{
	if (es_main() != GL_TRUE) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
