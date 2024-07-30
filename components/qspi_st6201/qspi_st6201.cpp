#ifdef USE_ESP_IDF
#include "qspi_st6201.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qspi_st6201 {

void qspi_st6201::setup() {
  esph_log_config(TAG, "Setting up qspi_st6201");
  this->spi_setup();
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
  }
  this->set_timeout(120, [this] { this->write_command_(SLEEP_OUT); });
  this->set_timeout(240, [this] { this->write_init_sequence_(); });
}

void qspi_st6201::update() {
  if (!this->setup_complete_) {
    return;
  }
  this->do_update_();
  // Start addresses and widths/heights must be divisible by 2 (CASET/RASET restriction in datasheet)
  if (this->x_low_ % 2 == 1) {
    this->x_low_--;
  }
  if (this->x_high_ % 2 == 0) {
    this->x_high_++;
  }
  if (this->y_low_ % 2 == 1) {
    this->y_low_--;
  }
  if (this->y_high_ % 2 == 0) {
    this->y_high_++;
  }
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->draw_pixels_at(this->x_low_, this->y_low_, w, h, this->buffer_, this->color_mode_, display::COLOR_BITNESS_565,
                       true, this->x_low_, this->y_low_, this->get_width_internal() - w - this->x_low_);
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void qspi_st6201::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (this->buffer_ == nullptr)
    this->init_internal_(this->width_ * this->height_ * 2);
  if (this->is_failed())
    return;
  uint32_t pos = (y * this->width_) + x;
  uint16_t new_color;
  bool updated = false;
  pos = pos * 2;
  new_color = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
  if (this->buffer_[pos] != (uint8_t) (new_color >> 8)) {
    this->buffer_[pos] = (uint8_t) (new_color >> 8);
    updated = true;
  }
  pos = pos + 1;
  new_color = new_color & 0xFF;

  if (this->buffer_[pos] != new_color) {
    this->buffer_[pos] = new_color;
    updated = true;
  }
  if (updated) {
    // low and high watermark may speed up drawing from buffer
    if (x < this->x_low_)
      this->x_low_ = x;
    if (y < this->y_low_)
      this->y_low_ = y;
    if (x > this->x_high_)
      this->x_high_ = x;
    if (y > this->y_high_)
      this->y_high_ = y;
  }
}

void qspi_st6201::reset_params_(bool ready) {
  if (!ready && !this->is_ready())
    return;
  this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
  // custom x/y transform and color order
  uint8_t mad = this->color_mode_ == display::COLOR_ORDER_BGR ? MADCTL_BGR : MADCTL_RGB;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;
  this->write_command_(MADCTL_CMD, &mad, 1);
  //this->write_command_(BRIGHTNESS, &this->brightness_, 1);
}

void qspi_st6201::write_init_sequence_() {

  this->write_command_(0xff,0xa5);
	 //TE_output_en
	this->write_command_(0xE7,0x10);
	 //TE_ interface_en //01
	this->write_command_(0x35,0x00);
	 //C8
	this->write_command_(0x36,0xC0);
	 //01---565/00---666
	this->write_command_(0x3A,0x01);
	 //01:IPS/00:TN
	this->write_command_(0x40,0x01);
	 //01--8bit//03--16bit
	this->write_command_(0x41,0x03);
	 //VBP 
	this->write_command_(0x44,0x15);
	 //VFP
	this->write_command_(0x45,0x15);
	 //vdds_trim[2:0]
	this->write_command_(0x7d,0x03);
	 //avdd_clp_en avdd_clp[1:0] avcl_clp_en avcl_clp[1:0] //0xbb 88 a2
	this->write_command_(0xc1,0xbb);
	 //vgl_clp_en vgl_clp[2:0]
	this->write_command_(0xc2,0x05);
	 //vgl_clp_en vgl_clp[2:0]
	this->write_command_(0xc3,0x10);
	 //avdd_ratio_sel avcl_ratio_sel vgh_ratio_sel[1:0] vgl_ratio_sel[1:0]
	this->write_command_(0xc6,0x3e);
	 //mv_clk_sel[1:0] avdd_clk_sel[1:0] avcl_clk_sel[1:0]
	this->write_command_(0xc7,0x25);
	 // VGL_CLK_sel
	this->write_command_(0xc8,0x21);
	 // user_vgsp //58
	this->write_command_(0x7a,0x51);
	 // user_gvdd //4F
	this->write_command_(0x6f,0x49);
	 // user_gvcl //70
	this->write_command_(0x78,0x57);
	
	this->write_command_(0xc9,0x00);
	
	this->write_command_(0x67,0x11);
	 //gate_st_o[7:0]
	this->write_command_(0x51,0x0a);
	 //gate_ed_o[7:0] //7A
	this->write_command_(0x52,0x7D);
	 //gate_st_e[7:0]
	this->write_command_(0x53,0x0a);
	 //gate_ed_e[7:0] //7A
	this->write_command_(0x54,0x7D);
	 //fsm_hbp_o[5:0] 
	this->write_command_(0x46,0x0a);
	 //fsm_hfp_o[5:0]
	this->write_command_(0x47,0x2a);
	 //fsm_hbp_e[5:0]
	this->write_command_(0x48,0x0a);
	 //fsm_hfp_e[5:0]
	this->write_command_(0x49,0x1a);
	 //VBP
	this->write_command_(0x44,0x15);
	 //VFP
	this->write_command_(0x45,0x15);
	
	this->write_command_(0x73,0x08);
	 //0A
	this->write_command_(0x74,0x10);
	 //src_ld_wd[1:0] src_ld_st[5:0]
	this->write_command_(0x56,0x43);
	 //pn_cs_en src_cs_st[5:0]
	this->write_command_(0x57,0x42);
	 //src_cs_p_wd[6:0]
	this->write_command_(0x58,0x3c);
	 //src_cs_n_wd[6:0]
	this->write_command_(0x59,0x64);
	 //src_pchg_st_o[6:0]
	this->write_command_(0x5a,0x41);
	 //src_pchg_wd_o[6:0]
	this->write_command_(0x5b,0x3C);
	 //src_pchg_st_e[6:0]
	this->write_command_(0x5c,0x02);
	 //src_pchg_wd_e[6:0]
	this->write_command_(0x5d,0x3c);
	 //src_pol_sw[7:0]
	this->write_command_(0x5e,0x1f);
	 //src_op_st_o[7:0] 
	this->write_command_(0x60,0x80);
	 //src_op_st_e[7:0]
	this->write_command_(0x61,0x3f);
	 //src_op_ed_o[9:8] src_op_ed_e[9:8]
	this->write_command_(0x62,0x21);
	 //src_op_ed_o[7:0]
	this->write_command_(0x63,0x07);
	 //src_op_ed_e[7:0]
	this->write_command_(0x64,0xe0);
	 //chopper
	this->write_command_(0x65,0x02);
	 //avdd_mux_st_o[7:0]
	this->write_command_(0xca,0x20);
	 //avdd_mux_ed_o[7:0]
	this->write_command_(0xcb,0x52);
	 //avdd_mux_st_e[7:0]
	this->write_command_(0xcc,0x10);
	 //avdd_mux_ed_e[7:0]
	this->write_command_(0xcD,0x42);
	 //avcl_mux_st_o[7:0]
	this->write_command_(0xD0,0x20);
	 //avcl_mux_ed_o[7:0]
	this->write_command_(0xD1,0x52);
	 //avcl_mux_st_e[7:0]
	this->write_command_(0xD2,0x10);
	 //avcl_mux_ed_e[7:0]
	this->write_command_(0xD3,0x42);
	 //vgh_mux_st[7:0]
	this->write_command_(0xD4,0x0a);
	 //vgh_mux_ed[7:0]
	this->write_command_(0xD5,0x32);
	 //gam_vrp0
	this->write_command_(0x80,0x00);
	 //gam_VRN0
	this->write_command_(0xA0,0x00);
	 //gam_vrp1 //07
	this->write_command_(0x81,0x06);
	 //gam_VRN1 //06 
	this->write_command_(0xA1,0x08);
	 //gam_vrp2 //02
	this->write_command_(0x82,0x03);
	 //gam_VRN2 //01
	this->write_command_(0xA2,0x03);
	 //gam_prp0 //11
	this->write_command_(0x86,0x14);
	 //gam_PRN0 //10
	this->write_command_(0xA6,0x14);
	 //gam_prp1 //27
	this->write_command_(0x87,0x2C);
	 //gam_PRN1 //27
	this->write_command_(0xA7,0x26);
	 //gam_vrp3
	this->write_command_(0x83,0x37);
	 //gam_VRN3
	this->write_command_(0xA3,0x37);
	 //gam_vrp4
	this->write_command_(0x84,0x35);
	 //gam_VRN4
	this->write_command_(0xA4,0x35);
	 //gam_vrp5
	this->write_command_(0x85,0x3f);
	 //gam_VRN5
	this->write_command_(0xA5,0x3f);
	 //gam_pkp0 //0b
	this->write_command_(0x88,0x0A);
	 //gam_PKN0 //0b
	this->write_command_(0xA8,0x0A);
	 //gam_pkp1 //14
	this->write_command_(0x89,0x13);
	 //gam_PKN1 //13
	this->write_command_(0xA9,0x12);
	 //gam_pkp2 //1a
	this->write_command_(0x8a,0x18);
	 //gam_PKN2 //1a
	this->write_command_(0xAa,0x19);
	 //gam_PKP3
	this->write_command_(0x8b,0x0a);
	 //gam_PKN3
	this->write_command_(0xAb,0x0a);
	 //gam_PKP4 //14
	this->write_command_(0x8c,0x17);
	 //gam_PKN4 //08
	this->write_command_(0xAc,0x0B);
	 //gam_PKP5 //17
	this->write_command_(0x8d,0x1A);
	 //gam_PKN5 //07
	this->write_command_(0xAd,0x09);
	 //gam_PKP6 //16 //16
	this->write_command_(0x8e,0x1A);
	 //gam_PKN6 //06 //13
	this->write_command_(0xAe,0x08);
	 //gam_PKP7 //1B
	this->write_command_(0x8f,0x1F);
	 //gam_PKN7 //07
	this->write_command_(0xAf,0x00);
	 //gam_PKP8 //04
	this->write_command_(0x90,0x08);
	 //gam_PKN8 //04
	this->write_command_(0xB0,0x00);
	 //gam_PKP9 //0A
	this->write_command_(0x91,0x10);
	 //gam_PKN9 //0A
	this->write_command_(0xB1,0x06);
	 //gam_PKP10 //16
	this->write_command_(0x92,0x19);
	 //gam_PKN10 //15
	this->write_command_(0xB2,0x15);
	this->write_command_(0xff,0x00);
	this->write_command_(0x11);
  	delay(120);
  	this->write_command_(0x29);
  	delay(20);
  	this->write_command_(0x36,0x48);
  	this->write_command_(0x20);
  this->reset_params_(true);
  this->setup_complete_ = true;
  esph_log_config(TAG, "qspi_st6201 setup complete");
}

void qspi_st6201::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  uint8_t buf[4];
  x1 += this->offset_x_;
  x2 += this->offset_x_;
  y1 += this->offset_y_;
  y2 += this->offset_y_;
  put16_be(buf, x1);
  put16_be(buf + 2, x2);
  this->write_command_(CASET, buf, sizeof buf);
  put16_be(buf, y1);
  put16_be(buf + 2, y2);
  this->write_command_(RASET, buf, sizeof buf);
}

void qspi_st6201::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                                display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (!this->setup_complete_ || this->is_failed())
    return;
  if (w <= 0 || h <= 0)
    return;
  if (bitness != display::COLOR_BITNESS_565 || order != this->color_mode_ ||
      big_endian != (this->bit_order_ == spi::BIT_ORDER_MSB_FIRST)) {
    return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                            x_pad);
  }
  this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
  this->enable();
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
    this->write_cmd_addr_data(8, 0x32, 24, 0x2C00, ptr, w * h * 2, 4);
  } else {
    this->write_cmd_addr_data(8, 0x32, 24, 0x2C00, nullptr, 0, 4);
    auto stride = x_offset + w + x_pad;
    for (int y = 0; y != h; y++) {
      this->write_cmd_addr_data(0, 0, 0, 0, ptr + ((y + y_offset) * stride + x_offset) * 2, w * 2, 4);
    }
  }
  this->disable();
}

void qspi_st6201::dump_config() {
  ESP_LOGCONFIG("", "QSPI ST6201:");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

}  // namespace qspi_st6201
}  // namespace esphome
#endif
