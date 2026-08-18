$ErrorActionPreference = 'Stop'

$imageName = 'tinyshell-os-dev:toolchain-v1'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

docker info | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw 'Docker engine is not available. Start Docker Desktop and retry.'
}

docker build --tag $imageName $repositoryRoot
if ($LASTEXITCODE -ne 0) {
    throw 'Docker image build failed.'
}

docker run --rm `
    --volume "${repositoryRoot}:/workspace" `
    --workdir /workspace `
    $imageName `
    make clean test
if ($LASTEXITCODE -ne 0) {
    throw 'TinyShell OS Docker test failed.'
}
