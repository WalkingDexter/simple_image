/*
Copyright © 2016 Andrey Tymchuk.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <phpcpp.h>      // Library PHP-CPP.
#include <Magick++.h>    // Library for work with images.
#include <iostream>    
#include <dirent.h>      // Library for work with directories.
#include <fstream>       // Library for work with file streams.
#include <sys/stat.h>    // Library for work with directories.
#include <vector>        // Library for work with vectors.
#include <pthread.h>     // Library for work with posix threads.

using namespace std; 
using namespace Magick;  // Magick namespace.

/**
 * Custom function for check, that file is exist.
 */
bool FileIsExist(std::string filePath) {
    bool isExist = false;
    std::ifstream fin(filePath.c_str());

    if(fin.is_open())
        isExist = true;

    fin.close();
    return isExist;
}

/**
 * Custom function for check, that extension is exist.
 */
bool ExtensionIsExist(std::string extension, std::string extensions[], int extensions_count) {
    bool isExist = false;
    
    int i = 0;
    while (!isExist && i < extensions_count) {
      if (extensions[i] == extension) {
        isExist = true;
      }
      i++;
    }
    
    return isExist;
}

// Sturct for result data.
struct ResultStruct {
    std::vector<std::string> images;   // Image file name vector.
    std::vector<double> diffs;        // Difference between images vector.
};

// Struct for thread argument.
struct ThreadArgs {
    intptr_t tid;                      // Thread unic id.
    std::vector<std::string> images;   // Images ti compare.
    std::string similar_image_dir;     // Simple image directory path.
    int width;                         // Image width.
    int small_width;                   // Small image width.
    int height;                        // Image height.
    int small_height;                  // Small image height.
    int range;                         // Image range.
    int small_range;                   // Small image range.
    PixelPacket *pixels;               // Image pixels packet.
    PixelPacket *small_pixels;         // Small image pixels packet.
};

// Struct for compare argument.
struct CompareArgs {
    Image compare_image;               // Image to compare. 
    int width;                         // Image width.
    int height;                        // Image height.
    int range;                         // Image range.
    PixelPacket *pixels;               // Image pixels packet.
};

double ImagesCompare(CompareArgs args) {
  
    double compare_diff = 100;

    Image compare_image = args.compare_image;
    
    int width = args.width;
    int height = args.height;
    int width2 = compare_image.columns();
    int height2 = compare_image.rows();
    double imagewh = (double)width / (double)height;
    double image2wh = (double)width2 / (double)height2;
    
    if ( (imagewh >= 1 && image2wh >= 1) || (imagewh <= 1 && image2wh <= 1) || (fabs(imagewh - image2wh) <= 0.1) ) {
        
        int range = args.range;
        int range2 = pow(2, compare_image.modulusDepth());
        if (range > 0 && range > 0) {
            compare_diff = 0;
            // Get a "pixel cache" for the entire image.
            PixelPacket *pixels = args.pixels;
            PixelPacket *pixels2 = compare_image.getPixels(0, 0, width2, height2);
            double max = pow(3 * pow(256, 2), 0.5);
            int pixelsCount = 0;
            int w = width > width2 ? width2 : width;
            int h = height > height2 ? height2 : height;
            for(int row = 0; row < h - 1; row++) {
                for(int column = 0; column < w - 1; column++) {
                    Color color = pixels[w * row + column];
                    Color color2 = pixels2[w * row + column];
                    double red = color.redQuantum() / range;
                    double green = color.greenQuantum() / range;
                    double blue = color.blueQuantum() / range;
                    double red2 = color2.redQuantum() / range2;
                    double green2 = color2.greenQuantum() / range2;
                    double blue2 = color2.blueQuantum() / range2;

                    compare_diff += pow(pow(red - red2, 2) + pow(green - green2, 2) + pow(blue - blue2, 2), 0.5) / max;
                    pixelsCount++;
                }
            }
            compare_diff = compare_diff / pixelsCount;
        }
    }
    return compare_diff;
}

/**
 * Thread function for compare images.
 */
void *ThreadFunction(void *arg) {
    ThreadArgs* args = reinterpret_cast<ThreadArgs*>(arg);
    
    std::vector<std::string> images = args->images;
    int size = args->images.size();
    
    // Our result struct.
    ResultStruct* result = new ResultStruct();
    std::vector<std::string> result_images;
    std::vector<double> result_diffs;
    
    double compare_diff;
    
    CompareArgs small_compare_args;
    small_compare_args.width = args->small_width;
    small_compare_args.height = args->small_height;
    small_compare_args.range = args->small_range;
    small_compare_args.pixels = args->small_pixels;
    
    CompareArgs compare_args;
    compare_args.width = args->width;
    compare_args.height = args->height;
    compare_args.range = args->range;
    compare_args.pixels = args->pixels;
    
    Image compare_image;
    
    
    try {
        for (int i = 0; i < size; i++) {
            std::string filename = images[i];
            std::string similar_image_dir = args->similar_image_dir;
            std::string image_path = similar_image_dir + "/" + filename;
            std::string small_image_path = similar_image_dir + "/4x4/" + filename;
            // Read a file into image object.
            compare_image.read(small_image_path);
            small_compare_args.compare_image = compare_image;
            compare_diff = ImagesCompare(small_compare_args);
            
            if (compare_diff <= 0.15) {
                compare_image.read(image_path);
                compare_args.compare_image = compare_image;
                compare_diff = ImagesCompare(compare_args);
                if (compare_diff <= 0.07) {
                    result_images.push_back(filename);
                    result_diffs.push_back(compare_diff);
                }
            }
        }
    }
    catch(Exception &error_) { 
        cout << "Caught exception: " << error_.what() << endl;
        compare_diff = -1;
    }
    
    result->images = result_images;
    result->diffs = result_diffs;

    return (void *) result;
}

/**
 *  Simple image index class for indexing our images.
 */
class SimpleImage : public Php::Base {
  private:
      /**
      *  The directory with images name value.
      *  @var    std:string
      */
      std::string image_dir;                              // Image directory name.
      std::string similar_image_dir;                      // Similar image directory, where we would save indexed images.
      static const int extensions_count = 4;              // Extensions images count.
      static const int NUM_THREADS = 4;                   // Posix threads count.
      std::string image_extensions[extensions_count] = {  // Available image extensions.
          "jpeg",
          "jpg",
          "png"
      };

  public:
    /**
     *  C++ constructor and destructor.
     */
    SimpleImage() {}
    virtual ~SimpleImage() {}
    
    /**
     *  php "constructor"
     *  @param  params
     */
    void __construct(Php::Parameters &params) {
        // Add new variable to avoid error "Сall of overloaded is ambiguous".
        std::string dir = params[0];
        std::string similar_dir = params[1];
        // Set directory from params.
        image_dir = dir;
        similar_image_dir = similar_dir;
    }
    
    /**
     *  Method for indexing images.
     */
    Php::Value simpleImageIndex() {
        // Define helpful variables.
        DIR *directory;
        struct dirent *entry;
        unsigned char isFile = 0x8;
        size_t pos;
        std::string extension;
        int indexed_images_count = 0;
        bool indexed;
        
        // Check if simple image directory exist.
        directory = opendir(similar_image_dir.c_str());
        if (directory) {
            closedir(directory);
        }
        else {
            mkdir(similar_image_dir.c_str(), ACCESSPERMS); 
        }
        
        // Check if simple image 4x4 directory exist.
        directory = opendir((similar_image_dir + "/4x4").c_str());
        if (directory) {
            closedir(directory);
        }
        else {
            mkdir((similar_image_dir + "/4x4").c_str(), ACCESSPERMS); 
        }
        
        // Try to open images directory.
        directory = opendir(image_dir.c_str());
        if (!directory) {
            perror("diropen");
            exit(1);
        };
        
        // Initialize magick++.
        InitializeMagick("");
        // Magick image object.
        Image image;
        
        // Get files from directory.
        while ( (entry = readdir(directory)) != NULL) {
            if (entry->d_type == isFile) {
                
                // Get filename and file extension.
                std::string filename(entry->d_name);
                pos = filename.find_last_of(".");
                extension = filename.substr(pos+1);
                
                // Check, that extension is exist.
                if (ExtensionIsExist(extension, image_extensions, extensions_count)) {
                  
                    indexed = false;
                    // Check that small size image is exist.
                    std::string original_image_path = image_dir + "/" + filename;
                    std::string indexed_image_path = similar_image_dir + "/" + filename;
                    if (!FileIsExist(indexed_image_path)) {
                        try {
                            // Read a file into image object.
                            image.read(original_image_path);
                            // Scale the image to specified size.
                            Geometry newSize = Geometry(32, 32);
                            image.scale(newSize);
                            // Use grayscale filter.
                            image.type(GrayscaleType);
                            // Write the image to a file.
                            image.write(indexed_image_path);
                            indexed = true;
                        }
                        catch(Exception &error_) { 
                            cout << "Caught exception: " << error_.what() << endl;
                            //return 1;
                        }
                    }
                    std::string indexed_small_image_path = similar_image_dir + "/4x4/" + filename;
                    if (!FileIsExist(indexed_small_image_path)) {
                        try {
                            image.read(indexed_image_path);
                            // Scale the image to specified size.
                            Geometry newSize = Geometry(4, 4);
                            image.scale(newSize);
                            // Use grayscale filter.
                            image.type(GrayscaleType);
                            // Write the image to a file.
                            image.write(indexed_small_image_path);
                            indexed = true;
                        }
                        catch(Exception &error_) { 
                            cout << "Caught exception: " << error_.what() << endl;
                        }
                    }
                    if (indexed) {
                      indexed_images_count++;
                    }
                }
            }
        };

        closedir(directory);
        
        return indexed_images_count;
    }
    
     /**
     *  Method for compare images.
     */
    Php::Value simpleImageCompare(Php::Parameters &params) {
      
        // Create an associative array.
        Php::Value assoc;
        
        // Get image name.
        std::string image_name = params[0];
        
         // Define helpful variables.
        DIR *directory;
        struct dirent *entry;
        unsigned char isFile = 0x8;
        size_t pos;
        std::string extension;
        std::vector<std::string> images;
        
        // Try to open images directory.
        directory = opendir(image_dir.c_str());
        if (!directory) {
            perror("diropen");
            exit(1);
        };
        
        // Initialize magick++.
        InitializeMagick("");
        Image image, small_image;
        
        try {
            image.read(similar_image_dir + "/" + image_name);
            small_image.read(similar_image_dir + "/4x4/" + image_name);
            
            // Read files from directory.
            while ((entry = readdir(directory)) != NULL) {
                if (entry->d_type == isFile) {
                  
                    // Get filename and file extension.
                    std::string filename(entry->d_name);
                    pos = filename.find_last_of(".");
                    extension = filename.substr(pos+1);
                    
                    // Check, that extension is exist.
                    if (ExtensionIsExist(extension, image_extensions, extensions_count) && image_name != filename) {
                        images.push_back(filename);
                    }
                }
            }
            
            // Run posix threads.
            intptr_t i;
            int rc;
            int images_count = images.size() / NUM_THREADS;
            pthread_attr_t attr;
            void *result;
            ThreadArgs* args[NUM_THREADS];
            
            pthread_t threads[NUM_THREADS];
            
            // Initialize and set thread joinable
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            
            for (i = 0; i < NUM_THREADS; i++) {
                args[i] = new ThreadArgs();
                
                args[i]->tid = i;
                int adds = images_count * i;
                if (i + 1 == NUM_THREADS) {
                    std::vector<std::string> images_part(images.begin() + adds, images.end());
                    args[i]->images = images_part;
                }
                else {
                    std::vector<std::string> images_part(images.begin() + adds, images.begin() + adds + images_count);
                    args[i]->images = images_part;
                }
                args[i]->similar_image_dir = similar_image_dir;
                args[i]->width = image.columns();
                args[i]->small_width = small_image.columns();
                args[i]->height = image.rows();
                args[i]->small_height = small_image.rows();
                args[i]->range = pow(2, image.modulusDepth());
                args[i]->small_range = pow(2, small_image.modulusDepth());
                args[i]->pixels = image.getPixels(0, 0, args[i]->width, args[i]->height);
                args[i]->small_pixels = small_image.getPixels(0, 0, args[i]->small_width, args[i]->small_height);
            
                rc = pthread_create(&threads[i], NULL, ThreadFunction, (void *)args[i]);
                if (rc){
                    cout << "Error: unable to create thread," << rc << endl;
                    exit(-1);
                }
            }
            
            // Join creating threads.
            pthread_attr_destroy(&attr);
            int response_size;
            for (i = 0; i < NUM_THREADS; i++){
               rc = pthread_join(threads[i], &result);
               if (rc) {
                  cout << "Error:unable to join," << rc << endl;
                  exit(-1);
               }
               else {
                 ResultStruct* response = reinterpret_cast<ResultStruct*>(result);
                 response_size = response->images.size();
                 for (int j = 0; j < response_size; j++) {
                     assoc[response->images[j]] = response->diffs[j];
                 }
               }
            }
        }
        catch(Exception &error_) { 
            cout << "Caught exception: " << error_.what() << endl;
        }
        
        closedir(directory);
        
        return assoc;
    }
};

/**
 *  tell the compiler that the get_module is a pure C function
 */
extern "C" {
    
    /**
     *  Function that is called by PHP right after the PHP process
     *  has started, and that returns an address of an internal PHP
     *  strucure with all the details and features of your extension
     *
     *  @return void*   a pointer to an address that is understood by PHP
     */
    PHPCPP_EXPORT void *get_module() {
        // static(!) Php::Extension object that should stay in memory
        // for the entire duration of the process (that's why it's static)
        static Php::Extension extension("example_extension", "1.0");
        //extension.add("simple_image_update_index", simple_image_update_index);
        
        // Description of the class so that PHP knows which methods are accessible.
        Php::Class<SimpleImage> simpleImage("SimpleImage");
        simpleImage.method("__construct", &SimpleImage::__construct);
        simpleImage.method("simpleImageIndex", &SimpleImage::simpleImageIndex);
        simpleImage.method("simpleImageCompare", &SimpleImage::simpleImageCompare);

        // Add the class to the extension.
        extension.add(std::move(simpleImage));
        
        // @todo    add your own functions, classes, namespaces to the extension
        
        // return the extension
        return extension;
    }
}
