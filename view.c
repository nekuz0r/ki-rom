/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "view.h"

view_t *view_current = (void *)0;

void view_switch(view_t *view)
{
    if (view_current != (void *)0)
    {
        view_current->unload();
    }
    view->load();
    view_current = view;
}
