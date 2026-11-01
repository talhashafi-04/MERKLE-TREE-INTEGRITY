# ============================================
# Merkle Tree Integrity System - Makefile
# ============================================

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto

# Directories
SRC_DIR = src
DATASET_DIR = datasets
PREPROC_DIR = pre_processed
BUILD_DIR = build
BIN_DIR = bin

# Files
SOURCE = $(SRC_DIR)/source.cpp
EXECUTABLE = $(BIN_DIR)/merkle_system
PREPROCESS_SCRIPT = $(SRC_DIR)/preprocess_data.py

RAW_DATA = $(DATASET_DIR)/raw_dataset.json
PREPROCESSED_DATA = $(PREPROC_DIR)/preprocessed_reviews.json
SAMPLE_DATA = $(PREPROC_DIR)/sample_reviews.json
MERKLE_ROOT = merkle_root.txt

# Python
PYTHON = python3

# Colors for output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[1;33m
BLUE = \033[0;34m
NC = \033[0m # No Color

# ============================================
# Main Targets
# ============================================

.PHONY: all clean run test preprocess sample help setup dirs

# Default target
all: dirs $(EXECUTABLE)
	@echo "$(GREEN)✓ Build complete!$(NC)"
	@echo "Run with: $(YELLOW)make run$(NC)"

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(PREPROC_DIR)
	@mkdir -p $(DATASET_DIR)

# Compile C++ source
$(EXECUTABLE): $(SOURCE) | dirs
	@echo "$(YELLOW)Compiling C++ source...$(NC)"
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(EXECUTABLE) $(LDFLAGS)
	@echo "$(GREEN)✓ Compilation successful$(NC)"

# Preprocess full dataset
preprocess: $(RAW_DATA)
	@echo "$(YELLOW)Preprocessing full dataset...$(NC)"
	@if [ ! -f "$(PREPROCESS_SCRIPT)" ]; then \
		echo "$(RED)✗ Preprocessing script not found: $(PREPROCESS_SCRIPT)$(NC)"; \
		exit 1; \
	fi
	cd $(dir $(PREPROCESS_SCRIPT)) && $(PYTHON) $(notdir $(PREPROCESS_SCRIPT))
	@if [ -f "preprocessed_reviews.json" ]; then \
		mv preprocessed_reviews.json ../$(PREPROC_DIR)/; \
	fi
	@echo "$(GREEN)✓ Preprocessing complete: $(PREPROCESSED_DATA)$(NC)"

# Create sample dataset
sample: $(RAW_DATA)
	@echo "$(YELLOW)Creating sample dataset (1000 records)...$(NC)"
	cd $(dir $(PREPROCESS_SCRIPT)) && $(PYTHON) $(notdir $(PREPROCESS_SCRIPT)) --sample
	@if [ -f "sample_reviews.json" ]; then \
		mv sample_reviews.json ../$(PREPROC_DIR)/; \
	fi
	@echo "$(GREEN)✓ Sample created: $(SAMPLE_DATA)$(NC)"

# Run the application
run: $(EXECUTABLE)
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@echo "$(BLUE)Launching Merkle Tree System...$(NC)"
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@./$(EXECUTABLE)

# Run with sample data (automated test)
test: $(EXECUTABLE) sample
	@echo "$(YELLOW)Running automated test with sample data...$(NC)"
	@echo "1\n$(SAMPLE_DATA)\n-1\n2\n3\n4\n9\n8\n100\n0" | ./$(EXECUTABLE)
	@echo "$(GREEN)✓ Test complete!$(NC)"

# Setup: Install dependencies
setup:
	@echo "$(YELLOW)Installing dependencies...$(NC)"
	@sudo apt-get update
	@sudo apt-get install -y build-essential libssl-dev
	@echo "$(GREEN)✓ Dependencies installed$(NC)"

# ============================================
# Clean Targets
# ============================================

# Clean compiled files only
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	rm -f $(MERKLE_ROOT)
	@echo "$(GREEN)✓ Build artifacts cleaned$(NC)"

# Clean preprocessed data
clean-data:
	@echo "$(YELLOW)Cleaning preprocessed data...$(NC)"
	rm -f $(PREPROC_DIR)/*.json
	@echo "$(GREEN)✓ Preprocessed data cleaned$(NC)"

# Clean everything except raw dataset
clean-all: clean clean-data
	@echo "$(GREEN)✓ All generated files cleaned$(NC)"
	@echo "$(BLUE)Raw dataset preserved: $(RAW_DATA)$(NC)"

# Nuclear clean - remove everything including raw data
clean-nuclear:
	@echo "$(RED)⚠ WARNING: This will delete ALL data including raw dataset!$(NC)"
	@read -p "Are you sure? (yes/no): " confirm; \
	if [ "$$confirm" = "yes" ]; then \
		rm -rf $(BUILD_DIR) $(BIN_DIR) $(PREPROC_DIR)/* $(DATASET_DIR)/*; \
		rm -f $(MERKLE_ROOT); \
		echo "$(RED)✓ Everything cleaned$(NC)"; \
	else \
		echo "$(YELLOW)Cancelled$(NC)"; \
	fi

# ============================================
# Utility Targets
# ============================================

# Show statistics
stats:
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@echo "$(BLUE)Project Statistics$(NC)"
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@if [ -f "$(RAW_DATA)" ]; then \
		echo "Raw Dataset:"; \
		echo "  File: $(RAW_DATA)"; \
		echo "  Size: $$(du -h $(RAW_DATA) | cut -f1)"; \
		echo "  Records: $$(wc -l < $(RAW_DATA) | xargs printf "%'d")"; \
		echo ""; \
	fi
	@if [ -f "$(PREPROCESSED_DATA)" ]; then \
		echo "Preprocessed Dataset:"; \
		echo "  File: $(PREPROCESSED_DATA)"; \
		echo "  Size: $$(du -h $(PREPROCESSED_DATA) | cut -f1)"; \
		echo "  Records: $$(wc -l < $(PREPROCESSED_DATA) | xargs printf "%'d")"; \
		echo ""; \
	fi
	@if [ -f "$(SAMPLE_DATA)" ]; then \
		echo "Sample Dataset:"; \
		echo "  File: $(SAMPLE_DATA)"; \
		echo "  Size: $$(du -h $(SAMPLE_DATA) | cut -f1)"; \
		echo "  Records: $$(wc -l < $(SAMPLE_DATA) | xargs printf "%'d")"; \
		echo ""; \
	fi
	@if [ -f "$(EXECUTABLE)" ]; then \
		echo "Executable:"; \
		echo "  File: $(EXECUTABLE)"; \
		echo "  Size: $$(du -h $(EXECUTABLE) | cut -f1)"; \
		echo ""; \
	fi
	@if [ -f "$(MERKLE_ROOT)" ]; then \
		echo "Merkle Root:"; \
		echo "  Stored: $$(cat $(MERKLE_ROOT))"; \
		echo ""; \
	fi

# Check if raw data exists
check-data:
	@if [ -f "$(RAW_DATA)" ]; then \
		echo "$(GREEN)✓ Raw dataset found: $(RAW_DATA)$(NC)"; \
	else \
		echo "$(RED)✗ Raw dataset not found: $(RAW_DATA)$(NC)"; \
		echo "$(YELLOW)Please place your raw_dataset.json in $(DATASET_DIR)/$(NC)"; \
		exit 1; \
	fi

# Rebuild everything
rebuild: clean all
	@echo "$(GREEN)✓ Rebuild complete$(NC)"

# ============================================
# Help Target
# ============================================

help:
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@echo "$(BLUE)Merkle Tree Integrity System - Makefile$(NC)"
	@echo "$(BLUE)════════════════════════════════════════════$(NC)"
	@echo ""
	@echo "$(YELLOW)Build Targets:$(NC)"
	@echo "  make all          - Compile the project (default)"
	@echo "  make rebuild      - Clean and rebuild everything"
	@echo ""
	@echo "$(YELLOW)Data Processing:$(NC)"
	@echo "  make preprocess   - Preprocess full dataset"
	@echo "  make sample       - Create 1000-record sample"
	@echo ""
	@echo "$(YELLOW)Run Targets:$(NC)"
	@echo "  make run          - Run the application"
	@echo "  make test         - Run automated test with sample"
	@echo ""
	@echo "$(YELLOW)Clean Targets:$(NC)"
	@echo "  make clean        - Remove build artifacts only"
	@echo "  make clean-data   - Remove preprocessed data"
	@echo "  make clean-all    - Remove all generated files"
	@echo "  make clean-nuclear- Remove EVERYTHING (including raw data)"
	@echo ""
	@echo "$(YELLOW)Utility Targets:$(NC)"
	@echo "  make setup        - Install dependencies"
	@echo "  make stats        - Show project statistics"
	@echo "  make check-data   - Check if raw data exists"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "$(YELLOW)Example Workflows:$(NC)"
	@echo "  $(GREEN)First time setup:$(NC)"
	@echo "    make setup"
	@echo "    make preprocess"
	@echo "    make all"
	@echo "    make run"
	@echo ""
	@echo "  $(GREEN)Quick test:$(NC)"
	@echo "    make test"
	@echo ""
	@echo "  $(GREEN)Clean rebuild:$(NC)"
	@echo "    make rebuild"
	@echo ""
