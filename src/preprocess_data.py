import json
import sys
import os

def generate_review_id(record, index):
    """Generate a unique review ID if not present"""
    reviewer = record.get('reviewerID', 'UNKNOWN')
    asin = record.get('asin', 'UNKNOWN')
    timestamp = record.get('unixReviewTime', index)
    return f"{reviewer}_{asin}_{timestamp}"

def clean_text(text):
    """Clean and normalize text fields"""
    if text is None:
        return ""
    text = str(text).replace('&amp;', '&').replace('&lt;', '<').replace('&gt;', '>')
    text = ' '.join(text.split())
    return text

def preprocess_reviews(input_file, output_file, limit=None):
    """Preprocess Amazon reviews for Merkle Tree construction"""
    
    print(f"Starting preprocessing of {input_file}...")
    print("=" * 60)
    
    processed_count = 0
    skipped_count = 0
    
    # Get absolute paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    input_path = os.path.join(project_root, 'datasets', input_file)
    output_path = os.path.join(project_root, 'pre_processed', output_file)
    
    with open(input_path, 'r', encoding='utf-8') as infile, \
         open(output_path, 'w', encoding='utf-8') as outfile:
        
        for line_num, line in enumerate(infile, 1):
            try:
                record = json.loads(line.strip())
                
                review_id = generate_review_id(record, line_num)
                
                processed_record = {
                    'review_id': review_id,
                    'reviewer_id': record.get('reviewerID', 'UNKNOWN'),
                    'product_id': record.get('asin', 'UNKNOWN'),
                    'rating': float(record.get('overall', 0.0)),
                    'verified': record.get('verified', False),
                    'review_text': clean_text(record.get('reviewText', '')),
                    'summary': clean_text(record.get('summary', '')),
                    'reviewer_name': clean_text(record.get('reviewerName', 'Anonymous')),
                    'timestamp': record.get('unixReviewTime', 0),
                    'review_time': record.get('reviewTime', 'Unknown'),
                    'vote': record.get('vote', '0')
                }
                
                outfile.write(json.dumps(processed_record) + '\n')
                processed_count += 1
                
                if processed_count % 100000 == 0:
                    print(f"Processed {processed_count:,} records...")
                
                if limit and processed_count >= limit:
                    print(f"\nReached limit of {limit:,} records")
                    break
                    
            except json.JSONDecodeError:
                skipped_count += 1
                continue
            except Exception as e:
                skipped_count += 1
                continue
    
    print("\n" + "=" * 60)
    print("PREPROCESSING COMPLETE")
    print("=" * 60)
    print(f"Total processed: {processed_count:,} records")
    print(f"Skipped: {skipped_count} records")
    print(f"Output file: {output_path}")
    
    return processed_count

def create_sample_output(input_file, sample_size=1000):
    """Create a small sample for testing"""
    output_file = "sample_reviews.json"
    print(f"\nCreating sample file with {sample_size} records...")
    count = preprocess_reviews(input_file, output_file, limit=sample_size)
    print(f"Sample file created: {output_file}")
    return count

if __name__ == "__main__":
    input_filename = "raw_dataset.json"
    output_filename = "preprocessed_reviews.json"
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "--sample":
            create_sample_output(input_filename, sample_size=1000)
        else:
            print("Usage:")
            print("  python3 preprocess_data.py           # Process full dataset")
            print("  python3 preprocess_data.py --sample  # Create 1000-record sample")
    else:
        count = preprocess_reviews(input_filename, output_filename)
