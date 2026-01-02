# Contributing to EspVault

Thank you for your interest in contributing to EspVault! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help create a welcoming environment for all contributors

## Development Setup

### Prerequisites

1. ESP-IDF v5.5 or later
2. Git
3. Python 3.7+
4. ESP32 development board with 4MB PSRAM

### Setup Instructions

```bash
# Clone repository
git clone https://github.com/thnak/EspVault.git
cd EspVault

# Install ESP-IDF (if not already installed)
# Follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# Set up environment
. $IDF_PATH/export.sh

# Build project
idf.py build
```

## Contributing Process

### 1. Fork and Clone

```bash
git clone https://github.com/YOUR_USERNAME/EspVault.git
cd EspVault
git remote add upstream https://github.com/thnak/EspVault.git
```

### 2. Create a Branch

```bash
git checkout -b feature/your-feature-name
```

Branch naming conventions:
- `feature/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Test additions or modifications

### 3. Make Changes

- Follow the coding style (see below)
- Add tests for new functionality
- Update documentation as needed
- Keep commits atomic and well-described

### 4. Test Your Changes

```bash
# Build and flash
idf.py build flash monitor

# Run unit tests (if available)
cd test
idf.py build flash monitor
```

### 5. Commit Changes

```bash
git add .
git commit -m "feat: add new feature description"
```

Commit message format:
- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `style:` - Code style changes (formatting)
- `refactor:` - Code refactoring
- `test:` - Test additions or changes
- `chore:` - Build/tooling changes

### 6. Push and Create Pull Request

```bash
git push origin feature/your-feature-name
```

Then create a pull request on GitHub.

## Coding Standards

### C/C++ Style

1. **Indentation**: 4 spaces (no tabs)
2. **Line Length**: Maximum 100 characters
3. **Naming Conventions**:
   - Functions: `snake_case`
   - Variables: `snake_case`
   - Constants: `UPPER_SNAKE_CASE`
   - Types: `snake_case_t`
   - Macros: `UPPER_SNAKE_CASE`

4. **Header Guards**: Use `#ifndef` style
   ```c
   #ifndef COMPONENT_NAME_H
   #define COMPONENT_NAME_H
   // ...
   #endif
   ```

5. **Documentation**: Use Doxygen-style comments
   ```c
   /**
    * @brief Brief description
    * 
    * Detailed description
    * 
    * @param param1 Description of param1
    * @return Description of return value
    */
   ```

### Example Code Style

```c
/**
 * @brief Calculate CRC8 checksum
 * 
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return uint8_t CRC8 value
 */
uint8_t vault_protocol_calc_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    
    return crc;
}
```

## Component Structure

When creating new components:

```
components/
└── component_name/
    ├── CMakeLists.txt
    ├── component_name.h
    ├── component_name.c
    └── README.md (optional)
```

### CMakeLists.txt Template

```cmake
idf_component_register(
    SRCS "component_name.c"
    INCLUDE_DIRS "."
    REQUIRES required_component1 required_component2
)
```

## Testing

- Add unit tests for new functionality
- Ensure existing tests pass
- Test on actual hardware when possible
- Document test procedures

## Documentation

- Update README.md for major features
- Add inline code documentation
- Create/update docs/ files for detailed guides
- Include examples where appropriate

## Pull Request Guidelines

### PR Title Format

```
[Type] Brief description
```

Examples:
- `[Feature] Add RMT peripheral support`
- `[Fix] Resolve sequence counter overflow`
- `[Docs] Update security configuration guide`

### PR Description Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Code refactoring

## Testing
- [ ] Tested on ESP32
- [ ] Tested on ESP32-S3
- [ ] Unit tests added/updated
- [ ] Documentation updated

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex code
- [ ] Documentation updated
- [ ] No new warnings generated
```

## Review Process

1. Automated checks run (build, style)
2. Code review by maintainers
3. Address feedback
4. Approval and merge

## Questions?

- Open an issue for bugs or feature requests
- Start a discussion for general questions
- Check existing issues before creating new ones

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

## Recognition

Contributors will be acknowledged in the project README and release notes.

Thank you for contributing to EspVault! 🚀
