import numpy as np
import os
from tensorflow.keras.models import Sequential, load_model
from tensorflow.keras.layers import LSTM, Dense
from tensorflow.keras.preprocessing.text import Tokenizer
from tensorflow.keras.preprocessing.sequence import pad_sequences
from sklearn.model_selection import train_test_split

def read_traces_from_files(directory):
    traces = []
    for filename in os.listdir(directory):
        if filename.endswith(".txt"):  # Assuming trace files are .txt
            with open(os.path.join(directory, filename), 'r') as file:
                traces.extend(file.read().splitlines())
    return traces

def preprocess_data(traces):
    tokenizer = Tokenizer()
    tokenizer.fit_on_texts(traces)
    total_words = len(tokenizer.word_index) + 1

    input_sequences = []
    for line in traces:
        token_list = tokenizer.texts_to_sequences([line])[0]
        for i in range(1, len(token_list)):
            n_gram_sequence = token_list[:i+1]
            input_sequences.append(n_gram_sequence)

    max_sequence_len = max([len(x) for x in input_sequences])
    input_sequences = np.array(pad_sequences(input_sequences, maxlen=max_sequence_len, padding='pre'))

    X, y = input_sequences[:,:-1], input_sequences[:,-1]
    y = np.array([y]).T

    return X, y, tokenizer, max_sequence_len, total_words

def create_model(max_sequence_len, total_words):
    model = Sequential()
    model.add(LSTM(100, input_shape=(max_sequence_len-1, 1)))
    model.add(Dense(total_words, activation='softmax'))
    model.compile(loss='sparse_categorical_crossentropy', optimizer='adam')
    return model

def train_model(X, y, model, epochs=100, batch_size=32):
    X_train, X_val, y_train, y_val = train_test_split(X, y, test_size=0.2, random_state=42)
    model.fit(X_train, y_train, epochs=epochs, batch_size=batch_size, validation_data=(X_val, y_val), verbose=1)
    return model

def predict_next(model, tokenizer, max_sequence_len, input_text):
    token_list = tokenizer.texts_to_sequences([input_text])[0]
    token_list = pad_sequences([token_list], maxlen=max_sequence_len-1, padding='pre')
    predicted = model.predict(token_list, verbose=0)
    predicted_word = ""
    for word, index in tokenizer.word_index.items():
        if index == np.argmax(predicted):
            predicted_word = word
            break
    return predicted_word

# Main execution
if __name__ == "__main__":
    # Directory containing trace files
    trace_directory = "./traces/"

    # Read traces from files
    traces = read_traces_from_files(trace_directory)

    # Preprocess data
    X, y, tokenizer, max_sequence_len, total_words = preprocess_data(traces)

    # Create and train the model
    model = create_model(max_sequence_len, total_words)
    model = train_model(X, y, model)

    # Save the model
    model.save("basic_block_predictor.h5")

    # Example prediction
    input_text = "<ls> + 0x7170"
    prediction = predict_next(model, tokenizer, max_sequence_len, input_text)
    print(f"Input: {input_text}")
    print(f"Predicted next block: {prediction}")

# To load and use the model later:
# loaded_model = load_model("basic_block_predictor.h5")
# prediction = predict_next(loaded_model, tokenizer, max_sequence_len, input_text)
