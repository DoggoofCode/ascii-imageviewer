#include <stdio.h> 
#include <stdlib.h>

typedef struct {
	int r;
	int g;
	int b;
	float alpha;
} TerminalPixel;


typedef struct {
	int r;
	int g;
	int b;
} Pixel;

typedef struct {
	int width;
	int height;
} ScreenSize;

Pixel* read_p6(char filename[]);
TerminalPixel* create_image_buffer(Pixel* raw_image_data, ScreenSize terminal_screen);
void draw(TerminalPixel* image_array, ScreenSize terminal_screen);
void clear();

int main(int argc, char *argv[]){
	if (argc < 2) {
		printf("No arguments provided, please provide atleast \x1B[1m1\x1B[0m argument\n");
		return 1;
	}
	ScreenSize TerminalScreen = {128, 64};

	Pixel *raw_image_data = read_p6(argv[1]);
	TerminalPixel *draw_buffer = create_image_buffer(raw_image_data, TerminalScreen);
	
	printf("First Pixel: %d, %d, %d\n", 
		raw_image_data[1].r,
		raw_image_data[1].g,
		raw_image_data[1].b
	);

	printf("First Pixel: %d, %d, %d %f\n", 
		draw_buffer[1].r,
		draw_buffer[1].g,
		draw_buffer[1].b,
		draw_buffer[1].alpha
	);
	clear();
	draw(draw_buffer, TerminalScreen);

	free(draw_buffer);
	free(raw_image_data);
	while (1){}
	return 0;
}

Pixel* read_p6(char filename[]){
	FILE *file_ptr;
	size_t file_size;
	char* buffer_start;
	char* buffer;
	int width, height;

	file_ptr = fopen(filename, "rb");
	// Check if the file opened successfully
	if (file_ptr == NULL) {
		printf("Error: Could not open file.\n");
		abort();
	}

	fseek(file_ptr, 0L, SEEK_END);
	file_size = ftell(file_ptr);
	rewind(file_ptr); // or fseek(file_ptr, 0L, SEEK_SET);

	buffer_start = calloc(file_size, 1); 
	buffer = buffer_start;

	if (buffer == NULL) {
		perror("Error allocating memory");
		fclose(file_ptr);
		abort();
	}

	size_t bytes_read = fread(buffer, 1, file_size, file_ptr);
	if (bytes_read != file_size) {
		perror("Error reading file content");
		abort();
	}
	
	for (int i = 0; i < 3; i++) {
		// Skip P6
		while (*buffer != '\x0a') {
			buffer++;
		}
		buffer++;
	}
	

	// TODO Must use strtol
	width = 256;
	height = 256;

	Pixel* raw_image_data = calloc(width*height, sizeof(Pixel));
	for (int index = 0; index < width*height; index++) {
		raw_image_data[index].r = (unsigned char)*(buffer++);
		raw_image_data[index].g = (unsigned char)*(buffer++);
		raw_image_data[index].b = (unsigned char)*(buffer++);
	}

	fclose(file_ptr);
	free(buffer_start);
	return raw_image_data;
}

TerminalPixel* create_image_buffer(Pixel* raw_image_data, ScreenSize terminal_screen){ TerminalPixel *draw_buffer = calloc(terminal_screen.width*terminal_screen.height, sizeof(TerminalPixel));
	int x_block_size = 256/terminal_screen.width;
	int y_block_size = 256/terminal_screen.height;
	int r; int g; int b;
	
	for (int y = 0; y < terminal_screen.height; y++){
		for (int x = 0; x < terminal_screen.width; x++){
			r = 0; g = 0; b = 0;
			// Sum Pixels
			for (int y_projected = y*y_block_size; y_projected < (y+1)*y_block_size; y_projected++){
				for (int x_projected = x*x_block_size; x_projected < (x+1)*x_block_size; x_projected++){
					size_t projected_array_position = 256*y_projected+x_projected;
					r += raw_image_data[projected_array_position].r;
					g += raw_image_data[projected_array_position].g;
					b += raw_image_data[projected_array_position].b;
				}
			}


			// Get Average Pixels
			r /= x_block_size*y_block_size;
			g /= x_block_size*y_block_size;
			b /= x_block_size*y_block_size;

			// Place into Image Buffer
			int array_idx = terminal_screen.width*y+x;
			draw_buffer[array_idx].r = r;
			draw_buffer[array_idx].g = g;
			draw_buffer[array_idx].b = b;
			draw_buffer[array_idx].alpha = ((float)(r+g+b))/765;
		}
	}

	return draw_buffer;
}

void draw(TerminalPixel* draw_buffer, ScreenSize terminal_screen){
	char alpha_list[] =  {'.', ':', '*', '%'};
	char alternative_alpha_list[] =  {'\'', ';', '=', '#'};
	char chosen;
	for (int y = 0; y < terminal_screen.height; y++){
		for (int x = 0; x < terminal_screen.width; x++){
			int array_idx = terminal_screen.width*y+x;
			int char_position = 0;
			if (draw_buffer[array_idx].alpha <= 0.25) {
				char_position = 0;
			} else if (draw_buffer[array_idx].alpha <= 0.5) {
				char_position = 1;
			} else if (draw_buffer[array_idx].alpha <= 0.75) {
				char_position = 2;
			} else if (draw_buffer[array_idx].alpha <= 1.0) {
				char_position = 3;
			}
			if (rand() >> (8*sizeof(int)-2) == 1) {
				chosen = alpha_list[char_position];
			} else {
				chosen = alternative_alpha_list[char_position];
			}
			//printf("%d", array_idx);
			printf("\x1B[38;2;%i;%i;%im%c\x1B[0m", draw_buffer[array_idx].r, draw_buffer[array_idx].g, draw_buffer[array_idx].b, chosen);
		}
		printf("\n");
	}
}

void clear() {
	printf("\x1B[H\x1B[2J");
}


