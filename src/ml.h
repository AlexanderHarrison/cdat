
#ifndef ML_H
#define ML_H

#include <stdint.h>
#include <stdbool.h>

// DATA TYPES ------------------------------------------------

typedef struct ML_F32 { union { float be_f; uint32_t be_u; } u; } ML_F32;
typedef struct ML_U32 { uint32_t be; } ML_U32;
typedef struct ML_U16 { uint16_t be; } ML_U16;
typedef struct ML_I32 { int32_t  be; } ML_I32;
typedef struct ML_I16 { int16_t  be; } ML_I16;
typedef uint8_t ML_U8;
typedef int8_t ML_I8;

typedef struct ML_Ref { ML_U32 offset; } ML_Ref;
#define ML_IsNull(A) (A.offset.be == 0)
#define ML_IsNonNull(A) (A.offset.be != 0)

static inline float ML_ReadF32(ML_F32 n) {
    n.u.be_u = dat_be32u(n.u.be_u);
    return n.u.be_f;
}
static inline uint32_t ML_ReadU32(ML_U32 n) { return dat_be32u(n.be); }
static inline uint16_t ML_ReadU16(ML_U16 n) { return dat_be16u(n.be); }
static inline int32_t  ML_ReadI32(ML_I32 n) { return (int32_t)dat_be32u((uint32_t)n.be); }
static inline int16_t  ML_ReadI16(ML_I16 n) { return (int16_t)dat_be16u((uint16_t)n.be); }
static inline uint8_t  ML_ReadU8 (ML_U8  n) { return n; }
static inline int8_t   ML_ReadI8 (ML_I8  n) { return n; }

static inline void *ML_ReadDatRef(DatFile *file, DatRef ref) {
    return (void *)(file->data + ref);
}

static inline DatRef ML_AsDatRef(ML_Ref ref) {
    return (DatRef)ML_ReadU32(ref.offset);
}

static inline void *ML_ReadRef(DatFile *file, ML_Ref ref) {
    return ML_ReadDatRef(file, ML_AsDatRef(ref));
}

// DECLARATIONS ------------------------------------------------

#define ML_Declare(A)\
    typedef struct ML_##A ML_##A;\
    typedef union ML_##A##Ref { ML_Ref ref; ML_U32 offset; } ML_##A##Ref;\
    static inline ML_##A *ML_ReadRef_##A(DatFile *file, ML_##A##Ref typ) { return ML_ReadRef(file, typ.ref); }
    
ML_Declare(Vec3)
ML_Declare(SList)
ML_Declare(Mtx)
ML_Declare(String)
ML_Declare(Bytes)
ML_Declare(GXColor)

ML_Declare(JObjDesc)
ML_Declare(DObjDesc)
ML_Declare(PObjDesc)
ML_Declare(MObjDesc)
ML_Declare(TObjDesc)
ML_Declare(RObjDesc)
ML_Declare(CObjDesc)
ML_Declare(PEDesc)
ML_Declare(Material)
ML_Declare(Spline)
ML_Declare(VtxDesc)

ML_Declare(MapHead)
ML_Declare(MapGObjDesc)
ML_Declare(JObjSet)
ML_Declare(ShapeSet)

// OBJECTS ------------------------------------------------

struct ML_GXColor {
    ML_U8 r, g, b, a;
};

struct ML_String { char c; };
struct ML_Bytes { ML_U8 b; };

struct ML_Vec3 {
    ML_F32 x, y, z;
};

struct ML_Mtx {
    ML_F32 data[3][4];
};

struct ML_JObjDesc {
    ML_StringRef class_name;
    
    // ML_JObjFlags
    ML_U32 flags;
    
    ML_JObjDescRef child;
    ML_JObjDescRef next;
    union {
        ML_DObjDescRef dobjdesc;
        ML_SplineRef spline;
        ML_SListRef particle;
    } vis;
    ML_Vec3 rotation;
    ML_Vec3 scale;
    ML_Vec3 position;
    ML_MtxRef envelope_mtx;
    ML_RObjDescRef robjdesc;
};

struct ML_DObjDesc {
    ML_StringRef class_name;
    ML_DObjDescRef next;
    ML_MObjDescRef mobjdesc;
    ML_PObjDescRef pobjdesc;
};

struct ML_MObjDesc {
    ML_StringRef class_name;
    
    // ML_RenderFlags
    ML_U32 render_flags;
    
    ML_TObjDescRef tobjdesc;
    ML_MaterialRef material;
    ML_U32 unused;
    ML_PEDescRef pedesc;
};

struct ML_Material {
    ML_GXColor ambient;
    ML_GXColor diffuse;
    ML_GXColor specular;
    ML_F32 alpha;
    ML_F32 shininess;
};

struct ML_TObjDesc {
    ML_StringRef class_name;
    ML_TObjDescRef next;
    
    // ML_GXTexMapID
    ML_U32 id;
    
    // ML_GXTexGenSrc
    ML_U32 src;
    
    ML_Vec3 rotation;
    ML_Vec3 scale;
    ML_Vec3 position;
    
    // ML_GXTexWrapMode
    ML_U32 wrap_x;
    ML_U32 wrap_y;
    
    ML_U8 repeat_x;
    ML_U8 repeat_y;
    
    // ML_TObjFlags
    ML_U32 flags;
};

struct ML_PObjDesc {
    ML_StringRef class_name; // TODO - decomp and mex disagree here...
    ML_PObjDescRef next;
    ML_VtxDescRef vertexdesc_array;
    ML_U16 flags;
    ML_U16 display_count;
    ML_BytesRef display;
    union {
        ML_JObjDescRef parent_jobjdesc;
        ML_ShapeSetRef parent_shapeset;
        ML_SListRef envelope_list;
        ML_Ref unk_ref;
    } u;
};

struct ML_JObjSet {
    ML_JObjDescRef jobjdesc;
    ML_Ref unk_joint_anim;
    ML_Ref unk_material_anim;
    ML_Ref unk_shape_anim;
};

struct ML_MapHead {
    ML_Ref points;
    ML_I32 point_count;
    ML_MapGObjDescRef map_gobjdescs;
    ML_I32 map_gobjdesc_count;
    ML_Ref unk_splines;
    ML_I32 unk_spline_count;
    ML_Ref unk_lights;
    ML_I32 unk_light_count;
    ML_Ref unk_splinedescs;
    ML_I32 unk_splinedesc_count;
    ML_Ref unk_x28;
    ML_I32 unk_x28_count;
};

struct ML_MapGObjDesc {
    ML_JObjSet jobjset;
    ML_CObjDescRef cobjdesc;
    ML_Ref unk_x14;
    ML_Ref unk_lobj;
    ML_Ref unk_fog;
    ML_Ref coll_links;
    ML_I32 coll_links_count;
    ML_BytesRef anim_behaviour;
    ML_Ref unk_coll_links_2;
    ML_I32 unk_coll_links_2_count;
};

// ENUMS ------------------------------------------------

enum ML_JObjFlags {
    JOBJ_SKELETON = (1 << 0),
    JOBJ_SKELETON_ROOT = (1 << 1),
    JOBJ_ENVELOPE_MODEL = (1 << 2),
    JOBJ_CLASSICAL_SCALE = (1 << 3),
    JOBJ_HIDDEN = (1 << 4),
    JOBJ_PTCL = (1 << 5),
    JOBJ_MTX_DIRTY = (1 << 6),
    JOBJ_LIGHTING = (1 << 7),
    JOBJ_TEXGEN = (1 << 8),
    
    JOBJ_BILLBOARD = (1 << 9),
    JOBJ_VBILLBOARD = (2 << 9),
    JOBJ_HBILLBOARD = (3 << 9),
    JOBJ_RBILLBOARD = (4 << 9),
    JOBJ_BILLBOARD_FIELD = (JOBJ_BILLBOARD | JOBJ_VBILLBOARD | JOBJ_HBILLBOARD | JOBJ_RBILLBOARD),
    
    JOBJ_INSTANCE = (1 << 12),
    JOBJ_SPLINE = (1 << 14),
    JOBJ_PBILLBOARD = (1 << 13),
    JOBJ_FLIP_IK = (1 << 15),
    JOBJ_SPECULAR = (1 << 16),
    JOBJ_USE_QUATERNION = (1 << 17),
    
    // TODO figure out names for these
    JOBJ_UNK_B18 = (1 << 18),
    JOBJ_UNK_B19 = (1 << 19),
    JOBJ_UNK_B20 = (1 << 20),
    JOBJ_JOINT1 = (1 << 21),
    JOBJ_JOINT2 = (2 << 21),
    JOBJ_JOINT3 = (3 << 21),
    JOBJ_JOINT_FIELD = (JOBJ_JOINT1 | JOBJ_JOINT2 | JOBJ_JOINT3),
    
    JOBJ_USER_DEF_MTX = (1 << 23),
    JOBJ_MTX_INDEP_PARENT = (1 << 24),
    JOBJ_MTX_INDEP_SRT = (1 << 25),
    
    // TODO figure out names for these
    JOBJ_UNK_B26 = (1 << 26),
    JOBJ_UNK_B27 = (1 << 27),
    
    JOBJ_ROOT_OPA = (1 << 28),
    JOBJ_ROOT_XLU = (2 << 28),
    JOBJ_ROOT_TEXEDGE = (4 << 28),
    JOBJ_ROOT_FIELD = (JOBJ_ROOT_OPA | JOBJ_ROOT_XLU | JOBJ_ROOT_TEXEDGE),
};

enum ML_RenderFlags {
    RENDER_CONSTANT = (1 << 0),
    RENDER_VERTEX = (1 << 1),
    RENDER_DIFFUSE = (1 << 2),
    RENDER_SPECULAR = (1 << 3),
    RENDER_CHANNEL_FIELD = (RENDER_CONSTANT | RENDER_VERTEX | RENDER_DIFFUSE | RENDER_SPECULAR),
    
    RENDER_TEX0 = (1 << 4),
    RENDER_TEX1 = (1 << 5),
    RENDER_TEX2 = (1 << 6),
    RENDER_TEX3 = (1 << 7),
    RENDER_TEX4 = (1 << 8),
    RENDER_TEX5 = (1 << 9),
    RENDER_TEX6 = (1 << 10),
    RENDER_TEX7 = (1 << 11),
    RENDER_TEX_FIELD = (RENDER_TEX0 | RENDER_TEX1 | RENDER_TEX2 | RENDER_TEX3 | RENDER_TEX4 | RENDER_TEX5 | RENDER_TEX6 | RENDER_TEX7),
    
    RENDER_TOON = (1 << 12),
    
    RENDER_ALPHA_MAT = (1 << 13),
    RENDER_ALPHA_VTX = (2 << 13),
    RENDER_ALPHA_BOTH = (3 << 13),
    RENDER_ALPHA_FIELD = (RENDER_ALPHA_MAT | RENDER_ALPHA_VTX | RENDER_ALPHA_BOTH),
    
    RENDER_SHADOW = (1 << 26),
    RENDER_ZMODE_ALWAYS = (1 << 27),
    RENDER_NO_ZUPDATE = (1 << 29),
    RENDER_XLU = (1 << 30),
};

enum ML_TObjFlags {
    TOBJ_COORD_REFLECTION = (1 << 0),
    TOBJ_COORD_HILIGHT = (2 << 0),
    TOBJ_COORD_SHADOW = (3 << 0),
    TOBJ_COORD_TOON = (4 << 0),
    TOBJ_COORD_GRADATION = (5 << 0),
    TOBJ_COORD_FIELD = (7 << 0),
    
    TOBJ_LIGHTMAP_DIFFUSE = (1 << 4),
    TOBJ_LIGHTMAP_SPECULAR = (2 << 4),
    TOBJ_LIGHTMAP_AMBIENT = (4 << 4),
    TOBJ_LIGHTMAP_EXT = (8 << 4),
    TOBJ_LIGHTMAP_SHADOW = (16 << 4),
    TOBJ_LIGHTMAP_FIELD = (31 << 4),
    
    TOBJ_COLORMAP_ALPHA_MASK = (1 << 16),
    TOBJ_COLORMAP_RGB_MASK = (2 << 16),
    TOBJ_COLORMAP_BLEND = (3 << 16),
    TOBJ_COLORMAP_MODULATE = (4 << 16),
    TOBJ_COLORMAP_REPLACE = (5 << 16),
    TOBJ_COLORMAP_PASS = (6 << 16),
    TOBJ_COLORMAP_ADD = (7 << 16),
    TOBJ_COLORMAP_SUB = (8 << 16),
    TOBJ_COLORMAP_FIELD = (7 << 16),
    
    TOBJ_ALPHAMAP_ALPHA_MASK = (1 << 20),
    TOBJ_ALPHAMAP_BLEND = (2 << 20),
    TOBJ_ALPHAMAP_MODULATE = (3 << 20),
    TOBJ_ALPHAMAP_REPLACE = (4 << 20),
    TOBJ_ALPHAMAP_PASS = (5 << 20),
    TOBJ_ALPHAMAP_ADD = (6 << 20),
    TOBJ_ALPHAMAP_SUB = (7 << 20),
    TOBJ_ALPHAMAP_FIELD = (7 << 20),
    
    TOBJ_BUMP = (1 << 24),
};
#define TOBJ_MTX_DIRTY (1u << 31u),

enum ML_GXTexMapID {
    GX_TEXMAP0,
    GX_TEXMAP1,
    GX_TEXMAP2,
    GX_TEXMAP3,
    GX_TEXMAP4,
    GX_TEXMAP5,
    GX_TEXMAP6,
    GX_TEXMAP7,
    GX_MAX_TEXMAP,
    GX_TEXMAP_NULL = 0xFF,
    GX_TEX_DISABLE = 0x100,
};

enum GXTexGenSrc {
    GX_TG_POS,
    GX_TG_NRM,
    GX_TG_BINRM,
    GX_TG_TANGENT,
    GX_TG_TEX0,
    GX_TG_TEX1,
    GX_TG_TEX2,
    GX_TG_TEX3,
    GX_TG_TEX4,
    GX_TG_TEX5,
    GX_TG_TEX6,
    GX_TG_TEX7,
    GX_TG_TEXCOORD0,
    GX_TG_TEXCOORD1,
    GX_TG_TEXCOORD2,
    GX_TG_TEXCOORD3,
    GX_TG_TEXCOORD4,
    GX_TG_TEXCOORD5,
    GX_TG_TEXCOORD6,
    GX_TG_COLOR0,
    GX_TG_COLOR1,
};

#endif

