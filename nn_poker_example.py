#!/usr/bin/env python3
"""
Neural Network Value Approximation for Poker
DeepStack-style implementation using TensorFlow/Keras

This demonstrates how to train a neural network to approximate
poker hand values, which can then be used to speed up CFR solving.
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from typing import List, Tuple
import json

class PokerFeatureExtractor:
    """Extract features from poker game states for neural network input"""

    @staticmethod
    def extract_features(game_state: dict) -> np.ndarray:
        """
        Extract normalized features from a poker game state.

        Features:
        - Hand strength metrics (4 features)
        - Board texture (6 features)
        - Game state (5 features)
        Total: 15 features

        Args:
            game_state: Dictionary containing poker state info

        Returns:
            numpy array of shape (15,) with normalized features
        """
        features = []

        # Hand strength features (4)
        features.append(game_state.get('ehs', 0.5))  # Expected hand strength
        features.append(game_state.get('hp_positive', 0.0))  # Hand potential +
        features.append(game_state.get('hp_negative', 0.0))  # Hand potential -
        features.append(game_state.get('hand_rank', 0.5))  # Normalized hand rank

        # Board texture features (6)
        features.append(1.0 if game_state.get('has_pair', False) else 0.0)
        features.append(1.0 if game_state.get('has_flush_draw', False) else 0.0)
        features.append(1.0 if game_state.get('has_straight_draw', False) else 0.0)
        features.append(1.0 if game_state.get('is_monotone', False) else 0.0)
        features.append(float(game_state.get('num_suits', 0)) / 4.0)
        features.append(float(game_state.get('high_card', 0)) / 12.0)

        # Game state features (5)
        pot = game_state.get('pot', 20)
        starting_pot = game_state.get('starting_pot', 20)
        stack_p1 = game_state.get('stack_p1', 100)
        stack_p2 = game_state.get('stack_p2', 100)
        street = game_state.get('street', 1)

        features.append(pot / 500.0)  # Normalize pot size
        features.append(stack_p1 / 200.0)  # Normalize stack
        features.append(stack_p2 / 200.0)  # Normalize stack
        features.append(float(street) / 5.0)  # Normalize street (0-5)
        features.append(min(pot / starting_pot, 5.0) / 5.0)  # Pot growth ratio

        return np.array(features, dtype=np.float32)


class PokerValueNetwork:
    """Neural network to predict poker hand values"""

    def __init__(self, input_dim: int = 15):
        self.input_dim = input_dim
        self.model = self._build_model()

    def _build_model(self) -> keras.Model:
        """
        Build DeepStack-style value network

        Architecture:
        - Input: 15 features
        - Hidden: 4 layers of [512, 512, 256, 128] neurons
        - Output: 2 values (for player 1 and player 2)
        """
        model = keras.Sequential([
            # Input layer
            keras.layers.Input(shape=(self.input_dim,)),

            # Hidden layers with BatchNorm and Dropout
            keras.layers.Dense(512, activation='relu'),
            keras.layers.BatchNormalization(),
            keras.layers.Dropout(0.2),

            keras.layers.Dense(512, activation='relu'),
            keras.layers.BatchNormalization(),
            keras.layers.Dropout(0.2),

            keras.layers.Dense(256, activation='relu'),
            keras.layers.BatchNormalization(),
            keras.layers.Dropout(0.1),

            keras.layers.Dense(128, activation='relu'),
            keras.layers.BatchNormalization(),

            # Output layer (no activation for regression)
            keras.layers.Dense(2, name='values')
        ])

        # Compile with Adam optimizer and MSE loss
        model.compile(
            optimizer=keras.optimizers.Adam(learning_rate=0.001),
            loss='mse',
            metrics=['mae']
        )

        return model

    def train(self,
              X_train: np.ndarray,
              y_train: np.ndarray,
              X_val: np.ndarray = None,
              y_val: np.ndarray = None,
              epochs: int = 50,
              batch_size: int = 256):
        """
        Train the value network

        Args:
            X_train: Training features (N, 15)
            y_train: Training labels (N, 2) - values for both players
            X_val: Validation features (optional)
            y_val: Validation labels (optional)
            epochs: Number of training epochs
            batch_size: Batch size for training
        """
        callbacks = [
            keras.callbacks.EarlyStopping(
                monitor='val_loss' if X_val is not None else 'loss',
                patience=10,
                restore_best_weights=True
            ),
            keras.callbacks.ReduceLROnPlateau(
                monitor='val_loss' if X_val is not None else 'loss',
                factor=0.5,
                patience=5,
                min_lr=1e-6
            )
        ]

        validation_data = (X_val, y_val) if X_val is not None else None

        history = self.model.fit(
            X_train, y_train,
            validation_data=validation_data,
            epochs=epochs,
            batch_size=batch_size,
            callbacks=callbacks,
            verbose=1
        )

        return history

    def predict(self, features: np.ndarray) -> np.ndarray:
        """
        Predict values for given features

        Args:
            features: Input features (batch_size, 15) or (15,)

        Returns:
            Predicted values (batch_size, 2) or (2,)
        """
        if len(features.shape) == 1:
            features = features.reshape(1, -1)

        predictions = self.model.predict(features, verbose=0)

        if predictions.shape[0] == 1:
            return predictions[0]

        return predictions

    def save(self, filepath: str):
        """Save model to disk"""
        self.model.save(filepath)
        print(f"Model saved to {filepath}")

    def load(self, filepath: str):
        """Load model from disk"""
        self.model = keras.models.load_model(filepath)
        print(f"Model loaded from {filepath}")


def generate_synthetic_training_data(num_samples: int = 10000) -> Tuple[np.ndarray, np.ndarray]:
    """
    Generate synthetic training data for demonstration.

    In practice, you would generate this by:
    1. Creating random poker states
    2. Solving each with CFR
    3. Recording features -> CFR values

    Returns:
        X: Features (num_samples, 15)
        y: Values (num_samples, 2)
    """
    print(f"Generating {num_samples} synthetic training examples...")

    X = []
    y = []

    for _ in range(num_samples):
        # Random features
        features = np.random.rand(15).astype(np.float32)

        # Synthetic value calculation (simplified!)
        # In reality, these would come from CFR solutions
        ehs = features[0]
        pot_ratio = features[11]

        # Player 1 value (simplified formula)
        # Higher EHS = better for P1
        value_p1 = (ehs - 0.5) * pot_ratio * 20.0

        # Player 2 value is negative of P1 (zero-sum)
        value_p2 = -value_p1

        X.append(features)
        y.append([value_p1, value_p2])

    return np.array(X), np.array(y)


def load_cfr_training_data(filepath: str) -> Tuple[np.ndarray, np.ndarray]:
    """
    Load training data generated from CFR solutions

    Expected format (JSON):
    [
        {
            "features": {...},  # Game state features
            "values": [v1, v2]  # CFR-computed values
        },
        ...
    ]

    Args:
        filepath: Path to JSON file with training data

    Returns:
        X: Features array
        y: Values array
    """
    print(f"Loading training data from {filepath}...")

    with open(filepath, 'r') as f:
        data = json.load(f)

    X = []
    y = []

    extractor = PokerFeatureExtractor()

    for example in data:
        features = extractor.extract_features(example['features'])
        values = example['values']

        X.append(features)
        y.append(values)

    X = np.array(X, dtype=np.float32)
    y = np.array(y, dtype=np.float32)

    print(f"Loaded {len(X)} training examples")

    return X, y


def main():
    """Example usage of the poker value network"""

    print("="*60)
    print("Poker Value Network - DeepStack Style")
    print("="*60)
    print()

    # Generate synthetic data (in practice, use CFR-generated data)
    X_train, y_train = generate_synthetic_training_data(10000)
    X_val, y_val = generate_synthetic_training_data(2000)

    print(f"Training set: {X_train.shape}")
    print(f"Validation set: {X_val.shape}")
    print()

    # Create and train network
    print("Building neural network...")
    value_net = PokerValueNetwork(input_dim=15)

    print("\nModel architecture:")
    value_net.model.summary()
    print()

    print("Training network...")
    history = value_net.train(
        X_train, y_train,
        X_val, y_val,
        epochs=50,
        batch_size=256
    )

    print()
    print("Training complete!")
    print(f"Final training loss: {history.history['loss'][-1]:.4f}")
    print(f"Final validation loss: {history.history['val_loss'][-1]:.4f}")
    print()

    # Save model
    value_net.save('poker_value_network.h5')

    # Test prediction
    print("Testing prediction on random state...")
    test_features = np.random.rand(15).astype(np.float32)
    prediction = value_net.predict(test_features)

    print(f"Features: {test_features}")
    print(f"Predicted values: P1={prediction[0]:.2f}, P2={prediction[1]:.2f}")
    print(f"Sum (should be ~0 for zero-sum): {prediction.sum():.4f}")
    print()

    # Example: How to use in C++
    print("="*60)
    print("To use this network in C++:")
    print("="*60)
    print("""
1. Export to TensorFlow Lite or ONNX:
   ```python
   # Convert to TensorFlow Lite
   converter = tf.lite.TFLiteConverter.from_keras_model(value_net.model)
   tflite_model = converter.convert()
   with open('poker_value_net.tflite', 'wb') as f:
       f.write(tflite_model)
   ```

2. Load in C++ with TensorFlow Lite:
   ```cpp
   #include <tensorflow/lite/interpreter.h>
   #include <tensorflow/lite/model.h>

   class ValueNetworkCpp {
       std::unique_ptr<tflite::Interpreter> interpreter;

       void load_model(const char* filename) {
           auto model = tflite::FlatBufferModel::BuildFromFile(filename);
           tflite::ops::builtin::BuiltinOpResolver resolver;
           tflite::InterpreterBuilder(*model, resolver)(&interpreter);
           interpreter->AllocateTensors();
       }

       std::array<float, 2> predict(const std::array<float, 15>& features) {
           // Copy features to input tensor
           float* input = interpreter->typed_input_tensor<float>(0);
           std::copy(features.begin(), features.end(), input);

           // Run inference
           interpreter->Invoke();

           // Get output
           float* output = interpreter->typed_output_tensor<float>(0);
           return {output[0], output[1]};
       }
   };
   ```

3. Use in DeepStack-style solver:
   ```cpp
   class DeepStackSolver {
       ValueNetworkCpp value_net;

       double cfr_traverse_with_nn(Game& game, int depth) {
           if (depth == 0 || game.game_over()) {
               // Use neural network instead of solving further
               auto features = extract_features(game);
               auto values = value_net.predict(features);
               return values[0];  // Return value for current player
           }

           // Continue CFR traversal
           // ...
       }
   };
   ```
    """)

    print("\nDemo complete! Model saved to 'poker_value_network.h5'")


if __name__ == '__main__':
    main()
