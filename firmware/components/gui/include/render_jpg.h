/*
 * TJpgDec wrapper functions for decoding jpeg images.
 *
 * Copyright 2020 Dan Julio
 *
 * This file is part of firecam.
 *
 * firecam is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * firecam is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with firecam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef RENDER_JPG_H
#define RENDER_JPG_H


//
// JPG Render Constants
//

// Using tjpgd (http://elm-chan.org/fsw/tjpgd/00index.html)
#define TJPGD_WORK_BUF_LEN 3100


//
// JPG Render API
//
int render_init();
int render_jpeg_image(uint8_t* fb, uint8_t* jpeg, uint32_t jpeg_length, uint16_t src_width, uint16_t dst_width);

#endif /* RENDER_JPG_H */
