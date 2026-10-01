#ifndef HALO_VITA_VERTEX_PROGRAM_H
#define HALO_VITA_VERTEX_PROGRAM_H
#include <psp2/gxm.h>
#include <stdint.h>
#include <stddef.h>
/* Descriptor schema reused from Xita runtime/xv_shader.h (GPL). */
#define XV_CONST_STREAM 255
#define XV_MAX_STREAMS 4
#define XV_MAX_ATTRS 16
typedef struct { const char *name; uint8_t stream; uint16_t offset; uint8_t format,components,vreg; } xv_attr_desc_t;
typedef struct {
 const char *gxp; uint8_t nstreams; uint16_t stride[4]; uint8_t nattrs;
 const xv_attr_desc_t *attrs; int16_t c_base; uint16_t c_count;
 uint32_t func_va,decl_va,func_hash,func_size;
} xv_vs_desc_t;
struct halo_vita_vertex_program {
 const xv_vs_desc_t *desc; const SceGxmProgram *source;
 SceGxmShaderPatcherId id; SceGxmVertexProgram *program;
 const SceGxmProgramParameter *constants;
 unsigned constant_stream,attributes,stream_mask; int retained,registered;
};
/* Initialize object to zero; destruction requires all GPU use retired. */
int halo_vita_vertex_catalog_load(unsigned index,struct halo_vita_vertex_program *out);
int halo_vita_vertex_program_destroy(struct halo_vita_vertex_program *program);
/* Exact byte lookup; caller supplies readable bounded spans. Output is unchanged on failure. */
int halo_vita_vertex_catalog_find(const void *program,unsigned program_bytes,
    const void *declaration,unsigned declaration_bytes,unsigned *index);
int halo_vita_vertex_catalog_pair(unsigned program,unsigned declaration,unsigned *out);
/* Selected instruction program and declaration may originate from different handles. */
int halo_vita_vertex_shader_current(const struct halo_vita_vertex_program **out);
/* Device owner calls only after all context use has drained. */
int halo_vita_vertex_shaders_shutdown(void);
struct halo_vita_stream_view;
/* Bind raw streams and uniform constants inside a logical scene. Success does
 * not submit a draw; caller must bind a compatible fragment program next.
 * NULL persistent input uses the internal attribute snapshot bank; explicit
 * input must stay immutable until scene retirement. */
int halo_vita_vertex_shader_bind(size_t first,size_t count,const struct halo_vita_stream_view *persistent);
#endif
