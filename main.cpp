#include "arena.h"
#include "prng.h"
#include "MachineLearning.h"
#include "base.h"
#include <chrono>
#include <iostream>

int main() {

    mem_arena* perm_arena = arena_create(GiB(1), MiB(1));
    matrix* train_images = mat_load(perm_arena, 60000, 784, "train_images.mat");
    matrix* test_images = mat_load(perm_arena, 10000, 784, "test_images.mat");
    matrix* train_labels = mat_create(perm_arena, 60000, 10);
    matrix* test_labels = mat_create(perm_arena, 10000, 10);

    {
        matrix* train_labels_file = mat_load(perm_arena, 60000, 1, "train_labels.mat");
        matrix* test_labels_file = mat_load(perm_arena, 10000, 1, "test_labels.mat");

        for (u32 i = 0; i < 60000; i++) {
            u32 num = train_labels_file->data[i];
            train_labels->data[i * 10 + num] = 1.0f;
        }

        for (u32 i = 0; i < 10000; i++) {
            u32 num = test_labels_file->data[i];
            test_labels->data[i * 10 + num] = 1.0f;
        }
    }


    draw_mnist_digit(train_images->data);
    for (u32 i = 0; i < 10; i++) {
        printf("%.0f ", train_labels->data[i]);
    }
    printf("\n\n");

    model_context* model = model_create(perm_arena);
    create_mnist_model(perm_arena, model);
    
    model_compile(perm_arena, model);
    memcpy(model->input->val->data, train_images->data, sizeof(f32) * 784);
    model_feedforward(model);

 

    model_training_desc training_desc{ train_images, train_labels, test_images, test_labels,3,50,0.01f };
    auto train_start = std::chrono::high_resolution_clock::now();

    model_train(model, &training_desc);

    auto train_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> train_time = train_end - train_start;

    printf("Training time: %.2f seconds\n", train_time.count());

    memcpy(model->input->val->data, test_images->data, sizeof(f32) * 784);
    model_feedforward(model);

    printf("\n\n");


    arena_destroy(perm_arena);

    return 0;
}
