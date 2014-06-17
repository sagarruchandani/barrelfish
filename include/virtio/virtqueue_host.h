/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef VIRTIO_VIRTQUEUE_HOST_H
#define VIRTIO_VIRTQUEUE_HOST_H

#include <barrelfish/barrelfish.h>

#include <virtio/virtqueue.h>

/*
 * Extracted from the Virtio Specification 1.0
 * http://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.pdf
 *
 * VIRQUEUES:
 *
 * Size is determined by a 16bit integer.
 * The queue consists of a descriptor table, available ring, used ring
 * Each of the three parts are physically-contiguous in guest memory.
 *
 */

// forward definition
struct virtqueue_host;


/**
 * \brief allocates and initiates a new virtqueue structure with no vring mem
 *
 * \param dev    the VirtIO device of to allocate the virtqueues for
 * \param setup  pointer to the setup information
 * \param vq_num the number of virtqueues structures to allocate
 *
 * \returns SYS_ERR_OK on success
 */
errval_t virtio_vq_host_alloc(struct virtqueue_host ***vq,
                              struct virtqueue_setup *setup,
                              uint16_t vq_num);

/**
 * \brief allocates and initiates a new virtqueue structure
 *
 * \param vdev      the VirtIO device
 * \param vring_cap capability to be used for the vring
 * \param vq_id     id of the queue to initialize
 * \param buf_bits  size of the buffers (0 for none)
 *
 * \returns SYS_ERR_OK on success
 */
errval_t virtio_vq_host_init_vring(struct virtio_device *vdev,
                                   struct capref vring_cap,
                                   uint16_t vq_id,
                                   uint8_t buf_bits);

/**
 * \brief frees the resources of previously allocated virtqueues
 *
 * \param vq pointer to the virtqueue memory to be freed
 *
 * \returns SYS_ERR_OK on success
 */
errval_t virtio_vq_host_free(struct virtqueue_host *vq);

/*
void    *virtqueue_drain(struct virtqueue_host *vq, int *last);
int  virtqueue_reinit(struct virtqueue_host *vq, uint16_t size);
*/


/*
 * ===========================================================================
 * Getter functions for certain values of the virtqueue structure
 */

/**
 * \brief Returns the physical address of the vring.
 *
 * \param vq pointer to the virtqueue structure
 *
 * \returns the physical address of the vring
 */
lpaddr_t virtio_vq_host_get_vring_paddr(struct virtqueue_host *vq);


/**
 * \brief Returns the frame capability of the vring
 *
 * \param vq        pointer to the virtqueue structure
 * \param ret_cap   memory location where to store the capref
 */
void virtio_vq_host_get_vring_cap(struct virtqueue_host *vq,
                                    struct capref *ret_cap);

/**
 * \brief Returns the queue index of the virtqueue of the device
 *
 * \param vq pointer to the virtqueue structure
 *
 * \returns queue index
 */
uint16_t virtio_vq_host_get_queue_index(struct virtqueue_host *vq);

/**
 * \brief returns the alignment of the vring
 *
 * \param the virtqueue to get the alignment from
 *
 * \returns vring alignment
 */
lvaddr_t virtio_vq_host_get_vring_align(struct virtqueue_host *vq);

/**
 * \brief Returns the number of elements (number of descriptors)in the vring of
 *        this virtqueue
 *
 * \param vq pointer to the virtqueue structure
 *
 * \returns number of elements in the vring
 */
uint16_t virtio_vq_host_get_num_desc(struct virtqueue_host *vq);

/**
 * \brief Checks if the virtqueue is empty
 *
 * \param vq pointer to the virtqueue structure
 *
 * \returns 0 the queue is not empty
 *          1 the queue is empty
 */
bool virtio_vq_host_is_empty(struct virtqueue_host *vq);


/**
 * \brief Calculates the number of used descriptors in this queue
 *
 * \param vq pointer to the virtqueue structure
 *
 * \returns number of used descriptors
 */
uint16_t virtio_vq_host_get_num_avail(struct virtqueue_host *vq);


/*
 * ===========================================================================
 * Interrupt handling
 */

/**
 * \brief sends an interrupt to the guest that an event has happened on the
 *        queue
 *
 * \param vq virtqueue to send the interrupt on
 *
 * \returns SYS_ERR_OK on success
 */
errval_t virtio_vq_host_intr_send(struct virtqueue_host *vq);


/**
 * \brief masks the ring features out of a features bit mask
 *
 * \param features  the features to mask
 *
 * \returns bitmask of masked features
 */
static inline uint64_t virtio_vq_host_mask_features(uint64_t features)
{
    uint64_t mask;

    mask = (1 << VIRTIO_TRANSPORT_F_START) - 1;
    mask |= (1 << VIRTIO_RING_F_INDIRECT_DESC);
    mask |= (1 <<VIRTIO_RING_F_EVENT_IDX);

    return (features & mask);
}

/*
 * ===========================================================================
 * Virtqueue Queue Management
 */

/**
 * \brief Enqueues a new descriptor chain into the virtqueue
 *
 * \param vq     the virtqueue the descriptor chain gets enqueued in
 * \param bl     list of buffers to enqueue into the virtqueue
 * \param st     state associated with this descriptor chain
 * \param num_wr number of writable descriptors
 * \param num_rd number of readable descriptors
 *
 * \returns SYS_ERR_OK on success
 *          VIRTIO_ERR_* on failure
 */
errval_t virtio_vq_host_desc_enqueue(struct virtqueue_host *vq,
                                       struct virtio_buffer_list *bl,
                                       void *st,
                                       uint16_t writeable,
                                       uint16_t readable);

/**
 * \brief dequeues a descriptor chain form the virtqueue
 *
 * \param vq     the virtqueue to dequeue descriptors from
 * \param ret_bl returns the associated buffer list structure
 * \param ret_st returns the associated state of the queue list
 *
 * \returns SYS_ERR_OK when the dequeue is successful
 *          VIRTIO_ERR_NO_DESC_AVAIL when there was no descriptor to dequeue
 *          VIRTIO_ERR_* if there was an error
 */
errval_t virtio_vq_host_desc_dequeue(struct virtqueue_host *vq,
                                       struct virtio_buffer_list **ret_bl,
                                       void **ret_st);


/**
 * \brief polls the virtqueue
 *
 * \param vq         the virtqueue to dequeue descriptors from
 * \param ret_bl     returns the associated buffer list structure
 * \param ret_st     returns the associated state of the queue list
 * \param handle_msg flag to have messages handled
 *
 * \returns SYS_ERR_OK when the dequeue is successful
 *          VIRTIO_ERR_* if there was an error
 */
errval_t virtio_vq_host_poll(struct virtqueue_host *vq,
                               struct virtio_buffer_list **ret_bl,
                               void **ret_st,
                               uint8_t handle_msg);


#endif // VIRTIO_VIRTQUEUE_H
