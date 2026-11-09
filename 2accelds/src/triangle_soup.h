
#ifndef __2ACCELDS_TRIANGLE_SOUP_H__
#define __2ACCELDS_TRIANGLE_SOUP_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// FCG Framework
#include <FCG/Mesh/mesh.h>



//////
//
// Functions
//

/// Create eight disconnected triangles with clearly separated groups of four, pairs, and individual triangles.
[[nodiscard]] auto makeTriangleSoup () -> fcg::Mesh;


#endif // __2ACCELDS_TRIANGLE_SOUP_H__
