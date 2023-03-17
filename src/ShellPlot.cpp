/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <Shell.hh>

using namespace MultiShell;

PlotTask::~PlotTask() {
  // ...
}

static int s_round(int iy, int scale) {
  return (iy >= 0) ? ((iy + scale / 2) / scale) : ((iy - scale / 2) / scale);
}

void PlotTask::prepare() {
  bool bFirst = true;

  char y_min = 0;
  char y_max = 0;

  m_x_max = -1;

  for (int b = 0; b < m_owner_ds.count(); b++) {
    const ShellBuffer *B = m_owner_ds.item(b);
    if (m_x_max < B->count() - 1)
      m_x_max = B->count() - 1;

    const char *ptr = B->buffer();
    for (int i = 0; i < B->count(); i++) {
      char value = *ptr++;
      if (bFirst) {
	bFirst = false;
	y_min = value;
	y_max = value;
      } else if (y_min > value) {
	y_min = value;
      } else if (y_max < value) {
	y_max = value;
      }
    }
  }
  if (y_max - y_min > 100)
    m_scale = 5;
  else if (y_max - y_min > 50)
    m_scale = 3;
  else
    m_scale = 1;
  m_row = s_round(y_max, m_scale);
  m_row_final = s_round(y_min, m_scale);
  m_col = 0;
}

bool PlotTask::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  if (!m_owner_ds.count() || m_x_max < 0) { // hmm... no data
    finish();
    return true;
  }

  bool bDone = false;

  while (afw) {
    if (m_col < 4) {
      if (m_row % 10) { // just spaces
	stream.write(' ', afw);
	++m_col;
	continue;
      }
      if (!m_col) {     // y-axis value in this row
	snprintf(m_tmp, 5, "%4d", m_row * m_scale);
      }
      stream.write(m_tmp[m_col++], afw);
      continue;
    }

    int x = m_col - 4;
    if (x == m_x_max) {
      stream.write_eol(afw);

      if (m_row == m_row_final) {
	bDone = true;
	finish();
	break;
      }
      m_col = 0;
      --m_row;
      continue;
    }

    char bg = m_row ? ' ' : '-';
    if (m_col == 4) {
      bg = m_row ? '|' : '+';
    }

    int y_max = m_row * m_scale + m_scale / 2;
    int y_min = m_row * m_scale - m_scale / 2;

    for (int b = 0; b < m_owner_ds.count(); b++) {
      const ShellBuffer *B = m_owner_ds.item(b);
      if (x < B->count()) {
	const char *ptr = B->buffer() + x;

	char value = *ptr;
	if (value >= y_min && value <= y_max) {
	  bg = (b < 26) ? ('a' + b) : '?';
	}
      }
    }
    stream.write(bg, afw);
    ++m_col;
  }

  return bDone;
}

void ShellPlot::demo(int option, Shell& origin, Timer *timer) {
  switch (option) {
  case 0:
    {
      PlotTask *P = task();
      if (P) {
	ShellBuffer *B = Shell::tmp_buffer();
	if (!B) {
	  P->return_to_owner();
	} else {
	  for (int i = 0; i <= 60 && B->space(); i++) {
	    char b = -30 + i;
	    *B << b;
	  }
	  P->push(*B);
	  P->prepare();
	  origin << *P;
	}
      }
    }
    break;
  case 1:
    {
      PlotTask *P = task();
      if (P) {
	ShellBuffer *A = Shell::tmp_buffer();
	if (!A) {
	  P->return_to_owner();
	} else {
	  ShellBuffer *B = Shell::tmp_buffer();
	  if (!B) {
	    A->return_to_owner();
	    P->return_to_owner();
	  } else {
	    for (int i = 0; i <= 100; i++) {
	      char a = -10 + i;
	      char b = -90 + i;
	      if (i < 55 && A->space())
		*A << a;
	      if (B->space())
		*B << b;
	    }
	    P->push(*A);
	    P->push(*B);
	    P->prepare();
	    origin << *P;
	  }
	}
      }
    }
    break;
  case 2:
    {
      PlotTask *P = task();
      if (P) {
	for (int i = 0; i < 4; i++) {
	  ShellBuffer *B = Shell::tmp_buffer();
	  if (!B) break;

	  char s = 127;
	  char v = 0;
	  while (B->space()) {
	    *B << s;
	    char a = -s / 12 - v / (1 + 2 * i);
	    v += a;
	    s += v;
	  }
	  P->push(*B);
	}
	P->prepare();
	origin << *P;
      }
    }
    break;
  default:
    origin << m_options;
    break;
  }
}

