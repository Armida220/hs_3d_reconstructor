﻿#ifndef _HS_3D_RECONSTRUCTOR_IMAGE_OPENGL_WINDOW_HPP_
#define _HS_3D_RECONSTRUCTOR_IMAGE_OPENGL_WINDOW_HPP_

#include "hs_graphics/graphics_render/thumbnail_image_renderer.hpp"
#include "hs_graphics/graphics_render/position_icon_2d_renderer.hpp"

#include "gui/opengl_window.hpp"

namespace hs
{
namespace recon
{

class ImageOpenGLWindow : public OpenGLWindow
{
  Q_OBJECT
public:
  typedef float Float;
  typedef hs::graphics::render::ThumbnailImageRenderer<Float> ImageRenderer;
  typedef hs::graphics::render::PositionIcon2DRenderer<Float> IconRenderer;
  typedef ImageRenderer::ImageData ImageData;
  typedef ImageRenderer::ViewingTransformer ViewingTransformer;
  typedef IconRenderer::Position Position;
  typedef IconRenderer::PositionContainer PositionContainer;

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

private:
  typedef EIGEN_VECTOR(Float, 2) Vector2;
public:
  ImageOpenGLWindow(QWindow* parent = 0);
  ~ImageOpenGLWindow();

  void DisplayThumbnailImage(int width, int height,
                             const ImageData& thumbnail_image_data);
  void SetOriginImage(const ImageData& origin_image_data);
  void ClearImage();
  void SetIcon(const ImageData& icon_image_data,
               int icon_offset_x = 0, int icon_offset_y = 0);
  void SetPositions(const PositionContainer& positions);

protected:
  virtual void Render();
  virtual void Initialize();

protected:
  void resizeEvent(QResizeEvent* event);
  void wheelEvent(QWheelEvent* event);
private:
  void CenterView();

public:
  void OnMouseDragging(Qt::KeyboardModifiers state_key,
                       Qt::MouseButton mouse_button, QPoint offset);

private:
  ImageRenderer* image_renderer_;
  IconRenderer* icon_renderer_;
  ViewingTransformer viewing_transformer_;
  ImageData thumbnail_image_data_;
  ImageData origin_image_data_;
  ImageData icon_image_data_;
  int image_width_;
  int image_height_;
  int icon_offset_x_;
  int icon_offset_y_;
  bool need_reload_image_;
  bool need_clear_image_;
  bool need_set_origin_image_;
  bool need_set_icon_;
  bool need_set_positions_;
  PositionContainer positions_;
};

}
}

#endif
