#ifndef KOUEK_H_Z_BUF_RAS_H
#define KOUEK_H_Z_BUF_RAS_H

#include <rasterizer.h>

namespace kouek {

class HierarchicalZBufferRasterizer : virtual public Rasterizer {
  public:
    virtual ~HierarchicalZBufferRasterizer() {}

    static std::unique_ptr<Rasterizer> Create();
};

} // namespace kouek

#endif // !KOUEK_H_Z_BUF_RAS_H
