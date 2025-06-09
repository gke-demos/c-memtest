# Stage 1: Build the static C application
FROM gcc:latest AS builder

WORKDIR /app

# Copy the C source file
COPY main.c .

# Compile the C application statically
# -static: Links all libraries statically. This is crucial for the scratch image.
# -Os: Optimize for size.
# -s: Strip all symbol table and relocation information.
RUN gcc -static -Os -s main.c -o c_memtest

# Stage 2: Create the final, minimal image using scratch
FROM scratch

# Copy the statically compiled executable from the builder stage
COPY --from=builder /app/c_memtest /c_memtest

# Set the entrypoint to run your application
ENTRYPOINT ["/c_memtest"]