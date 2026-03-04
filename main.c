#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

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

typedef struct {
	int background_mode;
} CommandFlags;

Pixel* read_p6(char filename[], ScreenSize* original_screen_ptr);
TerminalPixel* create_image_buffer(Pixel* raw_image_data, ScreenSize terminal_screen, ScreenSize original_screen);
void draw(TerminalPixel* image_array, ScreenSize terminal_screen, int use_background);
void clear();
CommandFlags get_command_flags(int argc, char** argv);

const int FPS = 24;

int main(int argc, char *argv[]){
	if (argc < 2) {
		printf("No arguments provided, please provide atleast \x1B[1m1\x1B[0m argument\n");
		return 1;
	}
	ScreenSize OriginalImage = {0};
	ScreenSize TerminalScreen = {0};

	Pixel *raw_image_data = read_p6(argv[1], &OriginalImage);

	struct winsize w;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		printf("Cannot IOCTL");
		abort();
		return 1;
	}
	
	CommandFlags flgs = get_command_flags(argc, argv);

	// Terminal width to height ratios
	float term_wth = (float)w.ws_col / w.ws_row;
	float image_wth = (float)OriginalImage.width / OriginalImage.height;

	if (term_wth <= image_wth) {
		// Terminal width is the limiting factor
		TerminalScreen.width = w.ws_col - 1;
		TerminalScreen.height = OriginalImage.height * (float)TerminalScreen.width/OriginalImage.width;
		TerminalScreen.height /= 2;
	} else {
		// Terminal height is the limiting factor
		TerminalScreen.height = w.ws_row - 4;
		TerminalScreen.width = OriginalImage.width * (float)TerminalScreen.height/OriginalImage.height;
		TerminalScreen.width *= 2;
	}
	TerminalPixel *draw_buffer = create_image_buffer(raw_image_data, TerminalScreen, OriginalImage);

	clear();

	for(int frame_count = 0; frame_count < 1; frame_count++){
		printf("\x1B[H");
		printf("%d", flgs.background_mode);	
		draw(draw_buffer, TerminalScreen, flgs.background_mode);

		fflush(stdout);
		usleep(1000000/FPS);
	}
	printf("\x1B[?25h");

	free(draw_buffer);
	free(raw_image_data);
	return 0;
}

Pixel* read_p6(char filename[], ScreenSize* original_screen_ptr){
	FILE *file_ptr;
	size_t file_size;
	char* buffer_start;
	char* buffer;
	char* endptr;
	int width, height;
	char image_sizes[256];

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
	
	// Skip P6
	while (*buffer != '\x0a') {
		buffer++;
	}
	buffer++;
	
	// Get Width and Height
	int i = 0;

	while (*buffer != '\0' && *buffer != '\x0a') {
	    image_sizes[i++] = *(buffer++);
	}
	buffer++;
	image_sizes[i] = '\0';

	width = strtol(image_sizes, &endptr, 10);
	height = strtol(endptr, NULL, 10);

	// Max Value 
	while (*buffer != '\x0a') {
		buffer++;
	}
	buffer++;

	original_screen_ptr->width = width;
	original_screen_ptr->height = height;

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

TerminalPixel* create_image_buffer(Pixel* raw_image_data, ScreenSize terminal_screen, ScreenSize original_screen){ 
	TerminalPixel *draw_buffer = calloc(terminal_screen.width*terminal_screen.height, sizeof(TerminalPixel));
	float x_block_size = (float)original_screen.width/terminal_screen.width;
	float y_block_size = (float)original_screen.height/terminal_screen.height;
	int r, g, b, block_area;
	
	for (int y = 0; y < terminal_screen.height; y++){
		for (int x = 0; x < terminal_screen.width; x++){
			r = 0; g = 0; b = 0;
			// Sum Pixels
			for (int y_projected = y*y_block_size; y_projected < (int)(y*y_block_size+(int)y_block_size); y_projected++){
				for (int x_projected = x*x_block_size; x_projected < (int)(x*x_block_size+(int)x_block_size); x_projected++){
					size_t projected_array_position = (int)original_screen.width*(int)y_projected+(int)x_projected;
					r += raw_image_data[projected_array_position].r;
					g += raw_image_data[projected_array_position].g;
					b += raw_image_data[projected_array_position].b;
				}
			}


			// Get Average Pixels
			block_area = (int)x_block_size*(int)y_block_size;
			r /= block_area;
			g /= block_area;
			b /= block_area;

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

void draw(TerminalPixel* draw_buffer, ScreenSize terminal_screen, int use_background){
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
			if (use_background)
				char_position = 3 - char_position;

			if (rand() >> (8*sizeof(int)-2) == 1) {
				chosen = alpha_list[char_position];
			} else {
				chosen = alternative_alpha_list[char_position];
			}
			// printf("\x1B[38;2;%i;%i;%im%c\x1B[0m", (int)(draw_buffer[array_idx].r * 255), 0, 0, '#');
			if (!use_background)
				printf("\x1B[38;2;%i;%i;%im%c\x1B[0m", draw_buffer[array_idx].r, draw_buffer[array_idx].g, draw_buffer[array_idx].b, chosen);
			else
				printf("\x1B[48;2;%i;%i;%im\x1B[30m%c\x1B[0m", draw_buffer[array_idx].r, draw_buffer[array_idx].g, draw_buffer[array_idx].b, chosen);
		}
		printf("               ");
		printf("\n\r");
	}
}

void clear() {
	printf("\x1B[H\x1B[2J");
	printf("\x1B[?25l");
}

CommandFlags get_command_flags(int argc, char** argv) {
	CommandFlags flags = {0};
	// move to first argument 
	argv++;

	for (int i = 0; i < argc-1; i++) {
		if (**argv == '-') {
			if (!strcmp("-b", *argv))
				flags.background_mode = 1;
		}
		argv++;
	}

	return flags;
}
