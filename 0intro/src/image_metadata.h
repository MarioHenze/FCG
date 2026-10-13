
#ifndef __0INTRO_IMAGE_METADATA_H__
#define __0INTRO_IMAGE_METADATA_H__


//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Image;
}



//////
//
// Functions
//

/// Draw decoded CPU image metadata inside the current ImGui window without changing the image.
void displayImageMetadata (const fcg::Image &image);


#endif // ifndef __0INTRO_IMAGE_METADATA_H__
