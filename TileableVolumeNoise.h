
#ifndef D_TILEABLE3DNOISE
#define D_TILEABLE3DNOISE

#include "glm\gtc\noise.hpp"

class Tileable3dNoise
{
public:

	/// @return Tileable Worley noise value in [0, 1].
	/// @param p 3d coordinate in [0, 1], being the range of the repeatable pattern.
	/// @param cellCount the number of cell for the repetitive pattern.
	static float WorleyNoise(const glm::vec3& p, float cellCount);

	/// @return Tileable Perlin noise value in [0, 1].
	/// @param p 3d coordinate in [0, 1], being the range of the repeatable pattern.
	static float PerlinNoise(const glm::vec3& p, float frequency, int octaveCount);

private:

	///
	/// Worley noise function based on https://www.shadertoy.com/view/Xl2XRR by Marc-Andre Loyer
	///

	static float hash(float n);
	static float noise(const glm::vec3& x);
	static float Cells(const glm::vec3& p, float numCells);

};

#endif // D_TILEABLE3DNOISE

