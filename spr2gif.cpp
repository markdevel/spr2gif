#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <fstream>
#include "getopt.h"
#include "gdpp.h"


class invalid_format_exception : public std::exception {};


// @todo read flat images (haven't been used on the official client?)
std::vector<std::unique_ptr<GD::Image>> load_spr(std::istream& source_spr_stream)
{
	short magic;
	short version;
	short number_of_indexed_images;
	short number_of_rgba_Images;

	// read header
	source_spr_stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	if (magic != 0x5053) { // 'S' 'P' in little endian
		throw invalid_format_exception();
	}
	source_spr_stream.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (version != 0x0101 && version != 0x0200 && version != 0x201) {
		throw invalid_format_exception();
	}
	source_spr_stream.read(reinterpret_cast<char*>(&number_of_indexed_images), sizeof(number_of_indexed_images));
	if (version >= 0x0200) {
		source_spr_stream.read(reinterpret_cast<char*>(&number_of_rgba_Images), sizeof(number_of_rgba_Images));
	}

	// read palette
	auto pos = source_spr_stream.tellg();
	source_spr_stream.seekg(-1024, std::istream::end);
	GD::Image palette_image(1, 1);
	for (size_t i = 0; i < 256; ++i) {
		char color[4];
		source_spr_stream.read(color, sizeof(color));
		palette_image.ColorAllocate(color[0], color[1], color[2]);
	}
	palette_image.ColorTransparent(0);
	source_spr_stream.seekg(pos, std::ifstream::beg);

	// read indexed images
	auto images = std::vector<std::unique_ptr<GD::Image>>();
	for (short i = 0; i < number_of_indexed_images; ++i) {
		short width;
		short height;
		short length;
		bool is_compressed;

		source_spr_stream.read(reinterpret_cast<char*>(&width), sizeof(width));
		source_spr_stream.read(reinterpret_cast<char*>(&height), sizeof(height));
		if (version >= 0x0201) {
			source_spr_stream.read(reinterpret_cast<char*>(&length), sizeof(length));
			is_compressed = true;
		}
		else {
			length = width * height;
			is_compressed = false;
		}

		auto image = std::make_unique<GD::Image>(width, height);
		image->PaletteCopy(palette_image);
		image->ColorTransparent(0);
		int c = 0;
		for (short j = 0; j < length; ++j) {
			int run = 1;
			int index = source_spr_stream.get();
			if (index == 0 && is_compressed) {
				run = source_spr_stream.get();
				j++;
			}
			for (int k = 0; k < run; ++k) {
				gdImagePalettePixel(image->GetPtr(), c % width, c / width) = index;
				c++;
			}
		}
		images.push_back(std::move(image));
	}
	return std::move(images);
}

void save_as_gif(const GD::Image& image, const wchar_t* output_filename_prefix)
{
	std::wstring output_filename(output_filename_prefix + std::wstring(L".gif"));
	std::ofstream output_stream;
	output_stream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	output_stream.open(output_filename, std::ofstream::binary);
	image.Gif(output_stream);
	output_stream.close();
}

void act_to_gif(std::istream& source_act_stream, const std::vector<std::unique_ptr<GD::Image>>& images, const wchar_t* output_filename_prefix, int width, int height, bool scaling, bool loop, int actionIndex)
{
	short magic;
	short version;
	short number_of_actions;
	char unknown[10];

	// read header
	source_act_stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	if (magic != 0x4341) { // 'A' 'C' in little endian
		throw invalid_format_exception();
	}
	source_act_stream.read(reinterpret_cast<char*>(&version), sizeof(version)); // 1.1, 2.0, 2.1, 2.2, 2.3, 2.4 or 2.5 ?
	source_act_stream.read(reinterpret_cast<char*>(&number_of_actions), sizeof(number_of_actions));
	source_act_stream.read(unknown, sizeof(unknown));

	// read action
	for (short i = 0; i < number_of_actions; ++i) {
		std::wostringstream output_filename;
		int buffer_size;
		void* buffer;
		int number_of_frames;
		GD::Image previous_frame;

		if (actionIndex != -1 && actionIndex != i) {
			continue;
		}
		if (actionIndex == -1) {
			output_filename << output_filename_prefix << L"_" << (int)i << L".gif";
		}
		else {
			output_filename << output_filename_prefix << L".gif";
		}
		std::ofstream output_gif_filestream;
		output_gif_filestream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
		output_gif_filestream.open(output_filename.str(), std::ofstream::binary);
		source_act_stream.read(reinterpret_cast<char*>(&number_of_frames), sizeof(number_of_frames));
		for (int j = 0; j < number_of_frames; ++j) {
			int number_of_sub_frames;
			int event_index;
			int number_of_pivots;

			source_act_stream.seekg(+32, std::istream::cur); // Range 0-1
			source_act_stream.read(reinterpret_cast<char*>(&number_of_sub_frames), sizeof(number_of_sub_frames));
			GD::Image frame(width, height);
			frame.PaletteCopy(*images[0]);
			frame.ColorTransparent(0);
			frame.FilledRectangle(0, 0, width, height, 0);
			for (int k = 0; k < number_of_sub_frames; ++k) {
				int offset_x;
				int offset_y;
				int image_index;
				int is_mirror;
				char color[4]; // r g b a
				float scale_x = 1.0f;
				float scale_y = 1.0f;
				int size_x;
				int size_y;
				int rotation;
				int type;

				source_act_stream.read(reinterpret_cast<char*>(&offset_x), sizeof(offset_x));
				source_act_stream.read(reinterpret_cast<char*>(&offset_y), sizeof(offset_y));
				source_act_stream.read(reinterpret_cast<char*>(&image_index), sizeof(image_index));
				source_act_stream.read(reinterpret_cast<char*>(&is_mirror), sizeof(is_mirror));
				if (version >= 0x0200) {
					source_act_stream.read(color, sizeof(color));
					source_act_stream.read(reinterpret_cast<char*>(&scale_x), sizeof(scale_x));
					if (version >= 0x0204) {
						source_act_stream.read(reinterpret_cast<char*>(&scale_y), sizeof(scale_y));
					}
					else {
						scale_y = scale_x;
					}
					source_act_stream.read(reinterpret_cast<char*>(&rotation), sizeof(rotation));
					source_act_stream.read(reinterpret_cast<char*>(&type), sizeof(type));
					if (version >= 0x0205) {
						source_act_stream.read(reinterpret_cast<char*>(&size_x), sizeof(size_x));
						source_act_stream.read(reinterpret_cast<char*>(&size_y), sizeof(size_y));
					}
					else {
						if (image_index >= 0) {
							size_x = images[image_index]->SX();
							size_y = images[image_index]->SY();
						}
						else {
							size_x = 0;
							size_y = 0;
						}
					}
				}
				if (!scaling) {
					scale_x = 1.0;
					scale_y = 1.0;
					offset_x = 0;
					offset_y = 0;
				}
				if (image_index >= 0) {
					if (scale_x == 1.0 && scale_y == 1.0) {
						int dstOffsetX = (frame.SX() - size_x) / 2 + offset_x;
						int dstOffsetY = (frame.SY() - size_y) / 2 + offset_y;
						frame.Copy(*images[image_index], dstOffsetX, dstOffsetY, 0, 0, size_x, size_y);
					}
					else {
						int dstOffsetX = (int)(frame.SX() - size_x * scale_x) / 2 + offset_x;
						int dstOffsetY = (int)(frame.SY() - size_y * scale_y) / 2 + offset_y;
						int dstSizeX = (int)(scale_x * (float)size_x);
						int dstSizeY = (int)(scale_y * (float)size_y);
						frame.CopyResized(*images[image_index], dstOffsetX, dstOffsetY, 0, 0, dstSizeX, dstSizeY, size_x, size_y);
					}
				}
			}
			if (j == 0) {
				buffer = frame.GifAnimBegin(&buffer_size, 1, loop ? 0 : -1);
				output_gif_filestream.write(reinterpret_cast<char*>(buffer), buffer_size);
				gdFree(buffer);
			}
			buffer = frame.GifAnimAdd(&buffer_size, 0, 0, 0, 25, gdDisposalRestoreBackground, nullptr);//previousFrame);
			output_gif_filestream.write(reinterpret_cast<char*>(buffer), buffer_size);
			gdFree(buffer);
			previous_frame = frame;

			if (version >= 0x0200) {
				source_act_stream.read(reinterpret_cast<char*>(&event_index), sizeof(event_index));  
				if (version >= 0x0203) {
					source_act_stream.read(reinterpret_cast<char*>(&number_of_pivots), sizeof(number_of_pivots));
					for (int m = 0; m < number_of_pivots; ++m) {
						float unknown4;
						int center_x;
						int center_y;
						int attribute;

						source_act_stream.read(reinterpret_cast<char*>(&unknown4), sizeof(unknown4));
						source_act_stream.read(reinterpret_cast<char*>(&center_x), sizeof(center_x));
						source_act_stream.read(reinterpret_cast<char*>(&center_y), sizeof(center_y));
						source_act_stream.read(reinterpret_cast<char*>(&attribute), sizeof(attribute));
					}
				}
			}
		}
		buffer = gdImageGifAnimEndPtr(&buffer_size);
		output_gif_filestream.write(reinterpret_cast<char*>(buffer), buffer_size);
		gdFree(buffer);
		output_gif_filestream.close();
	}

	if (version >= 0x0201) {
		int number_of_events;
		source_act_stream.read(reinterpret_cast<char*>(&number_of_events), sizeof(number_of_events));
		source_act_stream.seekg(40 * number_of_events, std::istream::cur); // event filenames
		for (int i = 0; i < number_of_actions; ++i) {
			float interval;
			source_act_stream.read(reinterpret_cast<char*>(&interval), sizeof(interval));
			// (int)(interval * 2.5); // @todo
		}
	}
}

void usage()
{
	std::cerr << "Usage: spr2gif [options] [actfile] sprfile\n"
		"Options:\n"
		"  -w --width=WIDTH    Specify the width of the output. (1-1024, default:32)\n"
		"  -h --height=HEIGHT  Specify the height of the output. (1-1024, default:32)\n"
		"  -s --scaling        Enable a scaling.\n"
		"  -l --loop           Enable a loop.\n"
		"  -n --index=N        Only output at N in the ACT file. (-1, 0-1023, default:-1)\n"
		"  -p --prefix=PREFIX  Specify the prefix of GIF files.\n"
		"\n";
}

int wmain(int argc, wchar_t** argv)
{
	static struct option_w long_options[] = {
		{ L"width",   required_argument, NULL, L'w' },
		{ L"height",  required_argument, NULL, L'h' },
		{ L"scaling", no_argument,       NULL, L's' },
		{ L"loop",    no_argument,       NULL, L'l' },
		{ L"index",   required_argument, NULL, L'n' },
		{ L"prefix",  required_argument, NULL, L'p' },
		{ 0 }
	};
	int width = 32;
	int height = 32;
	bool scaling = false;
	bool loop = false; // @todo notimpl
	int action_index = -1;
	std::wstring output_gif_filename_prefix;

	for (;;) {
		int option_index = 0;
		int c = getopt_long_w(argc, argv, L"w:h:sln:p:", long_options, &option_index);
		if (-1 == c) {
			break;
		}
		switch (c) {
		case L'w':
			width = _wtoi(optarg_w);
			break;

		case L'h':
			height = _wtoi(optarg_w);
			break;

		case L's':
			scaling = true;
			break;

		case L'l':
			loop = true;
			break;

		case L'n':
			action_index = _wtoi(optarg_w);
			break;

		case L'p':
			output_gif_filename_prefix = optarg_w;
			break;
		}
	}
	if (optind + 2 < argc || optind + 1 > argc || width < 1 || width > 1024 || height < 1 || height > 1024 || action_index < -1 || action_index > 1023) {
		usage();
		return EXIT_FAILURE;
	}
	wchar_t* source_act_filename = nullptr;
	if (optind + 2 == argc) {
		source_act_filename = argv[optind++];
	}
	std::wstring source_spr_filename(argv[optind++]);
	if (output_gif_filename_prefix.empty()) {
		std::wstring filename = source_spr_filename.substr(source_spr_filename.find_last_of(L"/\\") + 1);
		std::wstring basename = filename.substr(0, filename.find_last_of(L'.'));
		if (basename.empty()) {
			basename = L"output";
		}
		output_gif_filename_prefix = basename;
	}

	try {
		std::ifstream source_spr_filestream;
		source_spr_filestream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		source_spr_filestream.open(source_spr_filename, std::ifstream::binary);
		auto images = load_spr(source_spr_filestream);
		source_spr_filestream.close();

		if (nullptr == source_act_filename) {
			save_as_gif(*images[0], output_gif_filename_prefix.c_str());
		}
		else {
			std::ifstream source_act_filestream;
			source_act_filestream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
			source_act_filestream.open(source_act_filename, std::ofstream::binary);
			act_to_gif(source_act_filestream, images, output_gif_filename_prefix.c_str(), width, height, scaling, loop, action_index);
			source_act_filestream.close();
		}
	}
	catch (std::ifstream::failure& e) {
		std::cerr << e.what() << "\n";
	}
	catch (invalid_format_exception& e) {
		std::cerr << "The file format is invalid.\n";
	}
	return EXIT_SUCCESS;
}
