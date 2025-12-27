# Secure Voting System - Full Stack Application

A complete full-stack voting system built with **C++ backend** and **vanilla HTML/CSS/JavaScript frontend**. This project demonstrates professional software engineering practices including RESTful APIs, data persistence, input validation, and XSS prevention.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Features](#features)
3. [System Architecture](#system-architecture)
4. [Project Structure](#project-structure)
5. [Getting Started](#getting-started)
6. [Backend Setup & Compilation](#backend-setup--compilation)
7. [Frontend Setup](#frontend-setup)
8. [API Endpoints](#api-endpoints)
9. [Usage Examples](#usage-examples)
10. [Security Features](#security-features)
11. [How Voting Restriction Works](#how-voting-restriction-works)
12. [Troubleshooting](#troubleshooting)

---

## Project Overview

This voting system is designed to demonstrate a complete, production-ready implementation of:

- **RESTful API Server** in C++ using Crow framework
- **Modern Frontend UI** with responsive design
- **Data Persistence** using JSON files
- **Duplicate Vote Prevention** using voter IDs
- **Admin Panel** for candidate management
- **Real-time Vote Counting** with live updates
- **Input Validation** and security best practices

### Tech Stack

**Backend:**
- C++ 17 (standard)
- Crow Framework (lightweight HTTP server)
- nlohmann/json library (JSON serialization)
- Boost.Asio (networking)

**Frontend:**
- HTML5
- CSS3 (with modern features like CSS Grid)
- Vanilla JavaScript (no frameworks)
- Local Storage API
- Fetch API

---

## ✨ Features

### Voting Features
 **Multiple Candidates** - Support for unlimited candidates  
 **Vote Casting** - Submit votes through intuitive UI  
 **Duplicate Prevention** - Each voter ID can vote only once  
 **Live Results** - Real-time vote count updates  
 **Error Handling** - Clear error messages for invalid operations  

### Admin Features
 **Add Candidates** - Dynamically add candidates (password protected)  
 **Admin Authentication** - Simple password-based access control  
 **Audit Trail** - Track all voters who have participated  

### Technical Features
 **RESTful API** - Clean, standard API design  
 **CORS Support** - Cross-origin resource sharing enabled  
 **Data Persistence** - JSON-based local file storage  
 **Input Validation** - Server and client-side validation  
 **XSS Prevention** - HTML escaping and sanitization  
 **Responsive Design** - Works on desktop, tablet, and mobile  

---

##  System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    FRONTEND (Browser)                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  HTML5 + CSS3 + Vanilla JavaScript                   │   │
│  │  - Vote Interface                                    │   │
│  │  - Real-time Results Display                         │   │
│  │  - Admin Panel                                       │   │
│  │  - Local Storage (Voter ID persistence)             │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ↕
                   HTTP REST API (CORS)
                            ↕
┌─────────────────────────────────────────────────────────────┐
│                 BACKEND (C++ Server)                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Port: 8080                                          │   │
│  │  Framework: Crow                                     │   │
│  │                                                      │   │
│  │  Routes:                                             │   │
│  │  • GET  /candidates     - List all candidates        │   │
│  │  • POST /vote          - Cast a vote                 │   │
│  │  • POST /addCandidate  - Add new candidate (admin)   │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Controllers                                         │   │
│  │  └─ VotingController                                 │   │
│  │     • Vote validation                                │   │
│  │     • Duplicate prevention                           │   │
│  │     • Candidate management                           │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Models                                              │   │
│  │  ├─ Candidate (name, vote count)                     │   │
│  │  └─ Voter (ID, voted candidate)                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Data Persistence (JSON Files)                       │   │
│  │  ├─ data/candidates.json                             │   │
│  │  └─ data/voters.json                                 │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
voting1/
├── backend/
│   ├── main.cpp                      # Main server file with route handlers
│   ├── crow_all.h                    # Crow framework header
│   ├── models/
│   │   ├── Candidate.hpp            # Candidate model class
│   │   └── Voter.hpp                # Voter model class
│   ├── controllers/
│   │   └── VotingController.hpp      # Main voting logic controller
│   └── data/                         # Data storage (created at runtime)
│       ├── candidates.json           # Persistent candidates data
│       └── voters.json               # Persistent voters data
│
├── frontend/
│   ├── index.html                    # Main HTML interface
│   ├── style.css                     # Professional styling
│   └── script.js                     # Client-side logic & API calls
│
└── README.md                         # This file
```

---

## Getting Started

### Prerequisites

**For Backend:**
- C++17 compatible compiler (GCC, Clang, or MSVC)
- CMake (optional, for advanced builds)
- Boost libraries installed (for Asio)
- nlohmann/json library installed

**For Frontend:**
- Any modern web browser (Chrome, Firefox, Safari, Edge)
- No build tools required - just open HTML file

### Quick Start (5 minutes)

#### Step 1: Compile Backend
```bash
cd backend
g++ -std=c++17 -o voting_server main.cpp -I. -pthread
```

#### Step 2: Run Backend Server
```bash
./voting_server
# Output: Server starting on http://localhost:8080
```

#### Step 3: Open Frontend
```bash
# Open in browser:
file:///path/to/voting1/frontend/index.html
# Or serve with a simple HTTP server
cd frontend
python -m http.server 3000
# Then open: http://localhost:3000
```

That's it! The voting system is now running.

---

##  Backend Setup & Compilation

### Installing Dependencies

#### On Windows (MSVC)
```bash
# Install Visual C++ Build Tools
# Download and install: https://visualstudio.microsoft.com/visual-cpp-build-tools/

# Install Boost (using vcpkg)
vcpkg install boost-asio:x64-windows
vcpkg install nlohmann-json:x64-windows
```

#### On macOS
```bash
# Install Brew packages
brew install boost
brew install nlohmann-json

# Or using Conan
conan install . --build=missing
```

#### On Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install libboost-all-dev nlohmann-json3-dev g++ cmake
```

### Compilation Steps

**Using GCC (Linux/macOS):**
```bash
cd backend
g++ -std=c++17 -o voting_server main.cpp -I. -pthread -lstdc++fs
./voting_server
```

**Using MSVC (Windows):**
```bash
cd backend
cl /std:c++17 main.cpp /link ws2_32.lib
voting_server.exe
```

**Using Clang:**
```bash
cd backend
clang++ -std=c++17 -o voting_server main.cpp -I. -pthread
./voting_server
```

### Troubleshooting Compilation

If you get "crow_all.h not found":
- Make sure you're in the `backend/` directory
- The `crow_all.h` file must be in the same directory as `main.cpp`

If you get Boost library errors:
- Install Boost development files for your OS
- On Linux: `sudo apt-get install libboost-dev`
- On macOS: `brew install boost`

If you get JSON library errors:
- The `nlohmann/json.hpp` must be in your include path
- Download from: https://github.com/nlohmann/json/releases

---

## 💻 Frontend Setup

### Running the Frontend

**Option 1: Direct File Access**
```bash
# Simply open the HTML file in your browser
file:///path/to/voting1/frontend/index.html
```

**Option 2: Python HTTP Server (Recommended)**
```bash
cd frontend
python -m http.server 8000
# Open: http://localhost:8000
```

**Option 3: Node.js HTTP Server**
```bash
cd frontend
npx http-server
```

**Option 4: Using VSCode Live Server**
- Install "Live Server" extension
- Right-click `index.html` → "Open with Live Server"

### Frontend Files Overview

**index.html** - Main voting interface containing:
- Voter ID input
- Candidate selection (radio buttons)
- Vote submission button
- Live results display
- Admin panel for adding candidates
- Voting rules section

**style.css** - Professional styling featuring:
- Modern gradient header
- Responsive grid layout
- Smooth animations
- Mobile-friendly design
- Accessible color scheme
- CSS custom properties (variables)

**script.js** - Frontend logic including:
- API communication with backend
- Local storage management
- Real-time result updates
- Input validation
- Error/success message handling
- XSS prevention (HTML escaping)

---

## 🔌 API Endpoints

### Base URL
```
http://localhost:8080
```

### 1. GET /candidates
**Description:** Retrieve all candidates with their current vote counts.

**Request:**
```bash
curl -X GET http://localhost:8080/candidates
```

**Response (200 OK):**
```json
[
  {
    "name": "Alice Johnson",
    "votes": 5
  },
  {
    "name": "Bob Smith",
    "votes": 3
  },
  {
    "name": "Charlie Brown",
    "votes": 2
  }
]
```

---

### 2. POST /vote
**Description:** Cast a vote for a candidate. Each voter ID can only vote once.

**Request:**
```bash
curl -X POST http://localhost:8080/vote \
  -H "Content-Type: application/json" \
  -d '{
    "voter_id": "VOTER001",
    "candidate_name": "Alice Johnson"
  }'
```

**Response (200 OK - Success):**
```json
{
  "success": true,
  "message": "Vote recorded successfully for Alice Johnson",
  "candidates": [
    {"name": "Alice Johnson", "votes": 6},
    {"name": "Bob Smith", "votes": 3},
    {"name": "Charlie Brown", "votes": 2}
  ]
}
```

**Response (400 Bad Request - Error):**
```json
{
  "success": false,
  "message": "This voter ID has already voted"
}
```

**Error Cases:**
- `Voter ID cannot be empty` - Missing voter ID
- `Candidate name cannot be empty` - Missing candidate name
- `This voter ID has already voted` - Duplicate vote attempt
- `Candidate not found: [name]` - Invalid candidate name

---

### 3. POST /addCandidate
**Description:** Add a new candidate to the voting system (Admin only).

**Request:**
```bash
curl -X POST http://localhost:8080/addCandidate \
  -H "Content-Type: application/json" \
  -d '{
    "candidate_name": "Diana Prince",
    "admin_password": "admin123"
  }'
```

**Response (200 OK - Success):**
```json
{
  "success": true,
  "message": "Candidate added successfully: Diana Prince",
  "candidates": [
    {"name": "Alice Johnson", "votes": 6},
    {"name": "Bob Smith", "votes": 3},
    {"name": "Charlie Brown", "votes": 2},
    {"name": "Diana Prince", "votes": 0}
  ]
}
```

**Response (400 Bad Request - Error):**
```json
{
  "success": false,
  "message": "Invalid admin password"
}
```

**Admin Password:** `admin123` (default)

**Error Cases:**
- `Invalid admin password` - Wrong password provided
- `Candidate name cannot be empty` - Missing candidate name
- `Candidate already exists: [name]` - Duplicate candidate

---

## Usage Examples

### Example 1: Complete Voting Flow

```bash
# 1. Get all candidates
curl http://localhost:8080/candidates

# 2. Vote for a candidate
curl -X POST http://localhost:8080/vote \
  -H "Content-Type: application/json" \
  -d '{
    "voter_id": "USER123",
    "candidate_name": "Alice Johnson"
  }'

# 3. Check updated vote counts
curl http://localhost:8080/candidates
```

### Example 2: Add New Candidate (Admin)

```bash
# Add a new candidate
curl -X POST http://localhost:8080/addCandidate \
  -H "Content-Type: application/json" \
  -d '{
    "candidate_name": "Eve Wilson",
    "admin_password": "admin123"
  }'
```

### Example 3: Using JavaScript Fetch API

```javascript
// Get candidates
fetch('http://localhost:8080/candidates')
  .then(res => res.json())
  .then(data => console.log(data));

// Cast a vote
fetch('http://localhost:8080/vote', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    voter_id: 'USER123',
    candidate_name: 'Alice Johnson'
  })
})
.then(res => res.json())
.then(data => console.log(data));
```

### Example 4: Using Python Requests

```python
import requests
import json

API_URL = 'http://localhost:8080'

# Get candidates
response = requests.get(f'{API_URL}/candidates')
print(json.dumps(response.json(), indent=2))

# Cast a vote
payload = {
    'voter_id': 'USER123',
    'candidate_name': 'Alice Johnson'
}
response = requests.post(f'{API_URL}/vote', json=payload)
print(json.dumps(response.json(), indent=2))
```

---

## 🔐 Security Features

### 1. **Duplicate Vote Prevention**
- Each voter ID can vote only once
- Voter IDs stored in `voters.json` for persistence
- Server-side validation prevents duplicate votes

### 2. **Password-Protected Admin Panel**
- Only candidates added with correct password
- Default password: `admin123`
- Change in `VotingController.hpp` line 19

### 3. **Input Validation**
- Server-side validation for all inputs
- Client-side validation for better UX
- Rejects empty or invalid inputs

### 4. **XSS Prevention**
- HTML entity encoding on frontend
- No direct DOM manipulation with unsanitized input
- All user inputs properly escaped

### 5. **CORS Support**
- Configured for cross-origin requests
- Only necessary headers exposed
- Prevents unauthorized cross-site access

### 6. **Local Storage Security**
- Browser localStorage used only for voter ID
- No sensitive data stored client-side
- Voter ID used as reference only

---

## How Voting Restriction Works

### Frontend Logic

1. **Initial Check** (when page loads)
   - Checks browser's localStorage for key `votedVoterId`
   - If found, displays message: "You have already voted"
   - Disables voting interface

2. **Voter ID Input**
   - User enters their unique voter ID
   - ID can be username, email, passport number, etc.

3. **Vote Submission**
   - Frontend sends voter ID + candidate name to backend
   - Backend validates voter hasn't already voted
   - If valid, vote is recorded

4. **Post-Vote Storage**
   - Voter ID stored in browser localStorage
   - Prevents double-voting even after page refresh
   - Can be cleared by user through browser settings

### Backend Logic

1. **Voter Registration** (in `VotingController.hpp`)
   ```cpp
   // Check if voter exists in voters list
   auto voterExists = std::find_if(voters.begin(), voters.end(),
       [&voterId](const Voter& v) { return v.getVoterId() == voterId; });
   
   if (voterExists != voters.end()) {
       return {false, "This voter ID has already voted"};
   }
   ```

2. **Vote Recording**
   - Voter ID logged to `voters.json`
   - Prevents voting again in future sessions
   - Provides audit trail for election integrity

### Data Persistence

**voters.json** structure:
```json
[
  {
    "voter_id": "VOTER001",
    "candidate_name": "Alice Johnson"
  },
  {
    "voter_id": "VOTER002",
    "candidate_name": "Bob Smith"
  }
]
```

---

## Data Files

### candidates.json
Location: `backend/data/candidates.json`

```json
[
  {
    "name": "Alice Johnson",
    "votes": 5
  },
  {
    "name": "Bob Smith",
    "votes": 3
  }
]
```

**Automatically created** when you add the first candidate.

### voters.json
Location: `backend/data/voters.json`

```json
[
  {
    "voter_id": "VOTER001",
    "candidate_name": "Alice Johnson"
  },
  {
    "voter_id": "VOTER002",
    "candidate_name": "Bob Smith"
  }
]
```

**Automatically created** when the first vote is cast.

### Adding Initial Candidates

You can manually create `backend/data/candidates.json` with initial candidates:

```json
[
  {"name": "Alice Johnson", "votes": 0},
  {"name": "Bob Smith", "votes": 0},
  {"name": "Charlie Brown", "votes": 0}
]
```

Or use the admin panel in the frontend.

---

## Troubleshooting

### Problem: "Cannot GET /candidates" (404 Error)

**Solutions:**
1. Ensure backend server is running
   ```bash
   cd backend && ./voting_server
   ```
2. Check server is on port 8080
   - Look for: "Server starting on http://localhost:8080"
3. Check CORS is enabled (already included in code)

### Problem: Voter ID Not Persisting After Voting

**Solutions:**
1. Check browser localStorage is enabled
   - Some browsers block localStorage in private/incognito mode
2. Clear browser cache: Ctrl+Shift+Delete (or Cmd+Shift+Delete on Mac)
3. Try in a different browser

### Problem: "Invalid admin password" When Adding Candidate

**Solution:**
- Default password is: `admin123`
- Check for typos
- To change password, edit `VotingController.hpp` line 19

### Problem: Compilation Errors on Windows

**Solution:**
```bash
# Try using MSVC with appropriate flags
cl /std:c++17 /EHsc main.cpp /link ws2_32.lib
```

### Problem: Data Not Persisting Between Server Restarts

**Solution:**
- Check `data/` directory exists and contains JSON files
- Verify file permissions allow read/write
- Check files aren't corrupted (valid JSON)

### Problem: Port 8080 Already in Use

**Solution:**
1. Find what's using port 8080
2. Either kill that process or change port in `main.cpp`
   ```cpp
   app.port(8080).multithreaded().run();  // Change 8080 to another port
   ```

---

##  Code Quality & Design Patterns

### Object-Oriented Design
- **Models**: `Candidate` and `Voter` classes encapsulate data
- **Controller**: `VotingController` separates business logic from routes
- **Single Responsibility**: Each class has one reason to change

### SOLID Principles
- **S**ingle Responsibility: Each class does one thing
- **O**pen/Closed: Code open for extension (add methods) but closed for modification
- **L**iskov Substitution: Can swap implementations if needed
- **I**nterface Segregation: Models expose only necessary methods
- **D**ependency Inversion: High-level code doesn't depend on low-level details

### Code Documentation
- Comprehensive comments on all major functions
- Doxygen-style documentation blocks
- Clear variable names and logical structure

### Frontend Best Practices
- Semantic HTML5 markup
- CSS variables for maintainability
- Modular JavaScript functions
- Error handling and user feedback

---

##  Future Enhancements

Potential features to add:
1. **Database Integration** - Replace JSON files with SQLite/PostgreSQL
2. **Authentication** - User login system instead of voter IDs
3. **Real-time WebSocket** - Push updates instead of polling
4. **Encryption** - Secure voter ID transmission
5. **Admin Dashboard** - Web-based admin panel
6. **Email Notifications** - Confirm votes via email
7. **Docker Support** - Containerize application
8. **Unit Tests** - Comprehensive test suite
9. **Internationalization** - Multi-language support
10. **Analytics** - Voting statistics and graphs

---

##  License

This project is provided as-is for educational and demonstration purposes.

---

##  Support

For issues or questions:
1. Check the Troubleshooting section above
2. Verify all files are in correct directories
3. Ensure backend is running before opening frontend
4. Check browser console for JavaScript errors (F12)

---

##  Verification Checklist

Before running the system, ensure:

- [ ] Backend directory has: `main.cpp`, `crow_all.h`, `models/`, `controllers/`
- [ ] Frontend directory has: `index.html`, `style.css`, `script.js`
- [ ] C++ compiler installed and in PATH
- [ ] Backend compiled without errors
- [ ] Backend server running on port 8080
- [ ] Frontend accessible in browser
- [ ] Can see "Loading candidates..." message on page load
- [ ] Voting interface displays without errors
- [ ] Results update in real-time

---

**Enjoy your secure voting system! **



 * @file voting_server.cpp
 * @brief Voting System - Pure C++ Backend (Single-threaded)
 * 
 * Simple HTTP server for voting system
 * Compile: g++ -std=c++17 -o voting_server.exe voting_server.cpp -lws2_32


- command to run

## -->>>>        cd "c:\Users\Naren S J\Downloads\voting1" ; .\run.bat

