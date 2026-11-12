# DeepDownload-C
### Web Crawler and Document Downloader in C

## 1. Project Overview

DeepDownload-C is a web crawling and document downloading application developed using the C programming language.

The main purpose of this project is to automatically explore websites, discover useful documents, download them, identify duplicate files, organize the downloaded documents, and generate a report.

The system starts with a URL provided by the user. It visits webpages, extracts links, identifies supported document files, and processes them through an automated pipeline.

The project supports the discovery and downloading of:

- PDF (`.pdf`)
- Microsoft Word (`.docx`)
- ZIP archives (`.zip`)

Unlike a basic web crawler that only visits webpages and collects links, DeepDownload-C goes one step further by downloading and managing the documents found during crawling.

---

## 2. Project Objectives

The main objectives of this project are:

1. To understand how web crawling works internally.
2. To implement Breadth-First Search (BFS) for systematic webpage traversal.
3. To extract and process links from HTML pages.
4. To discover PDF, DOCX, and ZIP documents.
5. To download documents directly from websites.
6. To prevent unnecessary duplicate downloads.
7. To organize downloaded files automatically.
8. To filter documents using user-provided keywords.
9. To generate CSV reports containing document information.
10. To integrate all components into one automated pipeline.

---

## 3. Technologies Used

| Technology | Purpose |
|---|---|
| C (C11) | Main programming language |
| libcurl | HTTP and HTTPS communication |
| Makefile | Project compilation and build automation |
| SHA-256 | Document fingerprinting and duplicate detection |
| HTML processing | Extracting links from webpages |
| CSV | Generating document reports |
| Git and GitHub | Version control and collaboration |

---

## 4. How the Project Works

The project follows an automated process.

1. The user provides a starting website URL.
2. The crawler downloads the webpage.
3. HTML links are extracted.
4. New webpage URLs are added to a FIFO queue.
5. A visited URL structure prevents repeated processing.
6. The crawler follows links using Breadth-First Search.
7. Supported document links are identified separately.
8. Discovered documents are filtered using keywords, if configured.
9. Selected documents are downloaded.
10. SHA-256 hashes are calculated to identify duplicates.
11. Unique files are organized into folders.
12. A CSV report is generated.

### Overall Workflow

```text
             USER INPUT
                 |
                 v
          STARTING URL
                 |
                 v
           WEB CRAWLER
                 |
                 v
          HTML LINK EXTRACTION
                 |
          +------+------+
          |             |
          v             v
     WEBPAGE LINKS   DOCUMENT LINKS
          |             |
          v             v
      BFS QUEUE     DOCUMENT DISCOVERY
          |             |
          v             v
     VISIT PAGES    KEYWORD FILTERING
                        |
                        v
                  DOCUMENT DOWNLOAD
                        |
                        v
                   SHA-256 HASH
                        |
                        v
                 DUPLICATE CHECK
                        |
                        v
                 FILE ORGANIZATION
                        |
                        v
                   CSV REPORT
```

---

## 5. Phase-wise Development

The project was developed in multiple phases. Each phase introduced a new feature and built upon the previous implementation.

### Phase 1 – Project Foundation

Established the basic project structure, C source files, header files, and build configuration.

Created the foundation required to develop the project as separate reusable modules.

### Phase 2 – URL Processing

Implemented URL parsing, normalization, and resolution.

This allows the crawler to understand different URL formats and convert relative links into complete URLs.

Important functions:
- `url_parse()`
- `url_normalize()`
- `url_resolve()`

### Phase 3 – HTTP/HTTPS Communication

Integrated libcurl to communicate with websites.

Implemented webpage downloading and handling of HTTP responses.

This phase enabled the program to retrieve webpage content from remote servers.

### Phase 4 – HTML Link Extraction

Implemented HTML processing to identify hyperlinks present on downloaded webpages.

The extracted links become the input for further crawling.

### Phase 5 – Queue and Visited URL Management

Implemented two important data structures using Singly Linked Lists (SLL).

**Queue:**
- Follows FIFO (First In, First Out).
- Stores URLs waiting to be processed.
- Supports enqueue and dequeue operations.

**Visited URL List:**
- Stores URLs that have already been scheduled or attempted.
- Helps prevent unnecessary repeated processing.

### Phase 6 – Complete BFS Web Crawler

Integrated URL processing, HTTP communication, HTML extraction, queue management, and visited URL tracking.

Implemented Breadth-First Search (BFS) to visit webpages level by level.

The crawler starts from a given URL, extracts links, schedules new webpages, and continues until the configured crawling limits are reached.

Important functions:
- `crawler_run()`
- `crawler_run_with_link_observer()`

### Phase 7 – Crawler Safety

Added safety restrictions to make crawling more controlled.

Implemented:
- URL scope validation.
- Redirect destination validation.
- HTTPS downgrade prevention.
- Request timeout restrictions.
- Maximum response body size.
- Redirect limits.

Important function:
- `http_download_scoped()`

### Phase 8 – Document Discovery

Extended the crawler to identify supported documents separately from normal webpages.

The system identifies PDF, DOCX, and ZIP files using their URL extensions.

It also records the source webpage from which each document was discovered.

Important functions:
- `document_type_from_url()`
- `document_discover()`

### Phase 9 – Document Downloading

Implemented a dedicated document downloader.

Features include:
- Binary file downloading.
- Direct streaming of downloaded data to files.
- Maximum file-size restriction.
- HTTP response and content-type validation.
- Safe file creation without accidental overwriting.

The downloader uses streaming instead of loading the entire document into memory.

Important function:
- `download_document()`

### Phase 10 – Duplicate Detection and File Organization

Implemented SHA-256 hashing to identify duplicate documents.

The system calculates a fingerprint for each downloaded file and compares it with previously stored files.

When hashes match, the actual file contents are compared to confirm whether the files are identical.

Unique documents are organized into folders according to their file types.

Important functions:
- `sha256_file()`
- `organize_file()`

### Phase 11 – Keyword Filtering and CSV Reporting

Added keyword-based document filtering.

The system checks the user-provided keyword against:
- Document URLs.
- Source webpage URLs.

Matching is case-insensitive.

The system also generates CSV reports containing document information and source-document relationships.

Important functions:
- `report_row_matches()`
- `report_write_csv()`

Note: Keyword filtering operates on URLs, not on the internal contents of PDF or DOCX files.

### Phase 12 – Complete Pipeline Integration

Integrated all previously developed modules into one automated workflow.

The user provides the starting URL and configuration. The pipeline coordinates crawling, document discovery, filtering, downloading, duplicate detection, file organization, and CSV reporting.

Important functions:
- `pipeline_run()`
- `pipeline_result_free()`

This phase converts the individual modules into one complete application.

---

## 6. Data Structures Used

### Singly Linked List

Singly Linked Lists are used for queue and visited URL management.

Each node stores data and a pointer to the next node.

### FIFO Queue

The queue follows First In, First Out (FIFO).

The first URL inserted is the first URL processed.

This structure supports Breadth-First Search.

### Visited URL List

Maintains records of URLs that have already been scheduled or attempted.

It helps the crawler avoid repeatedly adding or processing the same URL.

**Why Singly Linked Lists?**

We selected Singly Linked Lists because they are simple to implement, easy to understand, and suitable for this educational project.

A hash table could provide faster average lookup for visited URLs, but linked lists offer a straightforward implementation with explicit memory management.

---

## 7. Project Features

- Automated website crawling.
- Breadth-First Search traversal.
- HTML link extraction.
- URL parsing and normalization.
- HTTP and HTTPS support through libcurl.
- URL scope and redirect safety checks.
- PDF, DOCX, and ZIP discovery.
- Document downloading through streaming.
- File-size restrictions.
- SHA-256-based duplicate detection.
- Automatic file organization.
- Keyword-based filtering.
- CSV report generation.
- Modular C implementation.

---

## 8. Project Structure

The project follows a modular structure, separating public interfaces from their implementations.

```text
DeepDownload-C/
|
|-- include/
|   |-- url.h
|   |-- queue.h
|   |-- visited.h
|   |-- http.h
|   |-- html.h
|   |-- project.h
|   |-- document.h
|   |-- download.h
|   |-- sha256.h
|   |-- organize.h
|   |-- report.h
|   |-- pipeline.h
|
|-- src/
|   |-- URL processing implementation
|   |-- Queue implementation
|   |-- Visited URL implementation
|   |-- HTTP implementation
|   |-- HTML processing implementation
|   |-- Crawler implementation
|   |-- Document discovery implementation
|   |-- Document downloading implementation
|   |-- SHA-256 implementation
|   |-- File organization implementation
|   |-- Reporting implementation
|   |-- Pipeline integration
|
|-- examples/
|   |-- deepdownload.c
|
|-- tests/
|
|-- Notes/
|
|-- Makefile
|-- README.md
```

*Note: This is a conceptual representation of the main modules. Refer to the actual repository for the complete file list.*

---

## 9. Building the Project

### Prerequisites

- C compiler supporting C11.
- GNU Make.
- libcurl development files and required dependencies.

### Compilation

Open the project directory in a terminal and run:

```bash
make
```

### Running Tests

```bash
make test
```

### Cleaning Build Files

```bash
make clean
```

The exact executable location and command-line arguments depend on the project's current Makefile and CLI implementation.

Check the project's usage instructions or executable help before running the application.

---

## 10. Output and Results

The application produces two main types of output.

### A. Organized Documents

Downloaded documents are stored in folders according to their file types.

Example:

```text
output/
|
|-- PDF/
|   |-- document_hash_1.pdf
|   |-- document_hash_2.pdf
|
|-- DOCX/
|   |-- document_hash_3.docx
|
|-- ZIP/
    |-- document_hash_4.zip
```

The actual filenames and directory layout are determined by the organizer implementation.

### B. CSV Report

A CSV report is generated containing information about discovered documents and their source webpages.

This makes it easier to review the documents found during crawling.

---

## 11. Limitations

The current implementation has the following limitations:

1. Only PDF, DOCX, and ZIP documents are supported.
2. Document type identification is based on URL extensions.
3. Keyword filtering checks document and source webpage URLs, not document contents.
4. SHA-256 is used for duplicate identification, with byte comparison when hashes match.
5. Crawling and downloading are subject to configured safety and resource limits.
6. Websites requiring JavaScript execution to generate their links may not expose those links to the HTML extraction process.
7. Authentication-protected pages may not be accessible without the required credentials.

---

## 12. Future Improvements

Possible future improvements include:

- Support for additional document formats.
- Content-based document type detection.
- Full-text document searching.
- More efficient URL lookup using hash tables.
- Parallel downloading.
- Improved retry and recovery mechanisms.
- More detailed reporting and statistics.
- A graphical user interface.

---

## 13. Conclusion

DeepDownload-C demonstrates how a complete web crawling and document downloading system can be developed using the C programming language.

The project combines fundamental concepts such as linked lists, queues, Breadth-First Search, HTTP communication, file handling, hashing, and modular programming.

Instead of only collecting webpage links, the system discovers documents, downloads them, identifies duplicates, organizes unique files, and generates reports.

The project helped us understand how different C modules can work together to create a practical application while maintaining control over memory management, network communication, and file operations.

---

**Project:** DeepDownload-C  
**Language:** C (C11)  
**Team:** Three Musketeers
