#pragma once

#include "slab.h"

enum notification_result {
        NOTIFICATION_RESULT_IGNORE = 0,
        NOTIFICATION_RESULT_WAKE = 1,
};

#define NOTIFICATION(NAME, ...)                                         \
        struct NAME {                                                   \
                const struct NAME##_notifier {                          \
                        struct slab_base *base;                         \
                        enum notification_result (*fn)                  \
                        (struct slab_reference,  ##__VA_ARGS__);        \
                } *op;                                                  \
                struct slab_reference ref;                              \
        }

#define NOTIFY(NOTIFICATION, ...)                                       \
        ({                                                              \
                __typeof__(NOTIFICATION)* notif_ = &(NOTIFICATION);     \
                if (notif_->op->fn(notif_->ref, ##__VA_ARGS__) !=        \
                    NOTIFICATION_RESULT_IGNORE)                         \
                        slab_notify(notif_->op->base, notif_->ref);     \
        })
