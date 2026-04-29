#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define SH_MAX_COEFFS 48

#ifdef DEBUG
#extension GL_EXT_debug_printf : enable
#define assert( condition, message, value ) if( ! bool(condition) ){ \
       debugPrintfEXT( message, value ); \
     }
#else
#define assert( condition, message, value )
#endif

#define MAGIC 0x4d415449u

const float lowest_alpha_coeff = 5.0f; // -ln(alpha_threshold)
const float inv_exp_lut_step = 31.0f/lowest_alpha_coeff;
const float lut[] = {
1.0,
0.8510449576692257,
0.7242775199742143,
0.616392731327227,
0.5245779259399984,
0.4464393987758162,
0.37993999923303906,
0.3233460205641274,
0.27518200038351043,
0.23419225386771747,
0.19930813677931222,
0.169620184828482,
0.1443544030172017,
0.1228520868051408,
0.10455264901465712,
0.08897900475488427,
0.07572513333507032,
0.0644454928936414,
0.054846011771641424,
0.046676421766522445,
0.03972373338644103,
0.0338066829983273,
0.0287710071012484,
0.024485420520582942,
0.020838193670452695,
0.017734239650173546,
0.015092635232377851,
0.012844511112456075,
0.010931256415982082,
0.009302990653810917,
0.007917263287169716,
0.006737946999085467
};

const float SH_C0 = 0.28209479177387814f;
const float SH_C1 = 0.4886025119029199f;
const float SH_C2[] = {
1.0925484305920792f,
-1.0925484305920792f,
0.31539156525252005f,
-1.0925484305920792f,
0.5462742152960396f
};
const float SH_C3[] = {
-0.5900435899266435f,
2.890611442640554f,
-0.4570457994644658f,
0.3731763325901154f,
-0.4570457994644658f,
1.445305721320277f,
-0.5900435899266435f
};

struct TileDepth {
    uint low;
    uint high;
};

struct Vertex {
    vec4 position;
    vec4 scale_opacity;
    vec4 rotation;
    float sh[48];
};

struct VertexAttribute {
    vec4 conic_opacity;
    vec4 color_radii;
    uvec4 aabb;
    vec2 uv;
    float depth;
    uint magic;
};

mat3 rotationFromQuaternion(vec4 q) {
    float qx = q.y;
    float qy = q.z;
    float qz = q.w;
    float qw = q.x;

    float qx2 = qx * qx;
    float qy2 = qy * qy;
    float qz2 = qz * qz;

    mat3 rotationMatrix;
    rotationMatrix[0][0] = 1 - 2 * qy2 - 2 * qz2;
    rotationMatrix[0][1] = 2 * qx * qy - 2 * qz * qw;
    rotationMatrix[0][2] = 2 * qx * qz + 2 * qy * qw;

    rotationMatrix[1][0] = 2 * qx * qy + 2 * qz * qw;
    rotationMatrix[1][1] = 1 - 2 * qx2 - 2 * qz2;
    rotationMatrix[1][2] = 2 * qy * qz - 2 * qx * qw;

    rotationMatrix[2][0] = 2 * qx * qz - 2 * qy * qw;
    rotationMatrix[2][1] = 2 * qy * qz + 2 * qx * qw;
    rotationMatrix[2][2] = 1 - 2 * qx2 - 2 * qy2;

    return rotationMatrix;
}